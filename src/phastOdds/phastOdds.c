#include <stdlib.h>
#include <stdio.h>
#include <getopt.h>
#include <misc.h>
#include <tree_model.h>
#include <hmm.h>
#include <msa.h>
#include <sufficient_stats.h>
#include <gff.h>
#include <bed.h>
#include <tree_likelihoods.h>

#define MIN_BLOCK_SIZE 30
/* used in identifying regions of missing data in a reference-sequence
   alignment */

/* to do: 
        - relax assumption that first seq is reference? 
        - -B, -b, -F, -f maybe shouldn't be options
*/

void usage(char *prog) {
  printf("\n\
PROGRAM:      %s\n\
\n\
DESCRIPTION:  Assign log-odds scores based on two phylo-HMMs, one\n\
              for features of interest (e.g., coding exons, conserved\n\
              regions) and one for background.  Default is to compute\n\
              a score for each feature in an input set, and to output\n\
              the same set of features, with newly computed scores\n\
              (output format is GFF by default; see -d).  The -w\n\
              option allows scores instead to be computed in a sliding\n\
              window of designated size.  In this case, the output is\n\
              a simple three-column text file, with the index of the\n\
              center of each window followed by the score for that\n\
              window on the positive strand, then the score for that\n\
              window on the negative strand.  Currently, a reference\n\
              sequence alignment is assumed in either case, with the\n\
              reference sequence appearing first; feature coordinates\n\
              are assumed to be defined with respect to the reference\n\
              sequence.\n\
\n\
USAGE:        phastOdds -B <backgd.hmm> -b <backgd_mods> \\\n\
                        -F <feat.hmm> -f <feat_mods> \\\n\
                        ( -g <feats.gff> | -w <size> ) \\\n\
                        [OPTIONS] <alignment_fname> \n\
\n\
              (alignment may be in any of the file formats listed below;\n\
               features may be formatted as GFF or BED)\n\
\n\
OPTIONS:\n\
    -B <backgd.hmm>   (Required) HMM file for background model\n\
    -b <backgd_mods>  (Required) Corresponding list of tree model files\n\
    -F <feat.hmm>     (Required) HMM file for feature model\n\
    -f <feat_mods>    (Required) Corresponding list of tree model files\n\
    -g <feats.gff>    (Required unless -w) File defining features to be\n\
                      scored (GFF or bed).\n\
    -w <size>         (Can be used instead of -g) Compute scores in a\n\
                      sliding window of the specified size.\n\
    -i <type>         Input format for alignment.  May be PHYLIP, FASTA,\n\
                      PSU, SS, or MAF (default PHYLIP)\n\
    -M <rseq.fa>      (For use with -i MAF) Reference sequence (FASTA)\n\
    -d                (For use with -g) Generate output in bed format rather\n\
                      than GFF.\n\
    -v                Verbose mode.  Print messages to stderr describing\n\
                      what the program is doing.\n\
    -h                Print this help message.\n\n", prog);
  exit(0);
}

int main(int argc, char *argv[]) {
  char c;
  List *l;
  int i, j, strand, bed_output = 0, backgd_nmods = -1, feat_nmods = -1, 
    winsize = -1, verbose = 0, max_nmods, memblocksize;
  TreeModel **backgd_mods = NULL, **feat_mods = NULL;
  HMM *backgd_hmm = NULL, *feat_hmm = NULL;
  msa_format_type inform = PHYLIP;
  GFF_Set *features = NULL;
  MSA *msa, *msa_compl;
  double **backgd_emissions, **feat_emissions, **mem, **dummy_emissions,
    *winscore_pos, *winscore_neg;
  int *no_alignment;

  while ((c = getopt(argc, argv, "B:b:F:f:g:w:i:M:dvh")) != -1) {
    switch (c) {
    case 'B':
      backgd_hmm = hmm_new_from_file(fopen_fname(optarg, "r"));
      break;
    case 'b':
      l = get_arg_list(optarg);
      backgd_nmods = lst_size(l);
      backgd_mods = smalloc(backgd_nmods * sizeof(void*));
      for (i = 0; i < backgd_nmods; i++) 
        backgd_mods[i] = tm_new_from_file(fopen_fname(((String*)lst_get_ptr(l, i))->chars, "r"));
      lst_free_strings(l); lst_free(l);
      break;
    case 'F':
      feat_hmm = hmm_new_from_file(fopen_fname(optarg, "r"));
      break;
    case 'f':
      l = get_arg_list(optarg);
      feat_nmods = lst_size(l);
      feat_mods = smalloc(feat_nmods * sizeof(void*));
      for (i = 0; i < feat_nmods; i++) 
        feat_mods[i] = tm_new_from_file(fopen_fname(((String*)lst_get_ptr(l, i))->chars, "r"));
      lst_free_strings(l); lst_free(l);
      break;
    case 'g':
      features = gff_read_set(fopen_fname(optarg, "r"));
      break;
    case 'w':
      winsize = atoi(optarg);
      if (winsize <= 0) die("ERROR: window size must be positive.\n");
      break;
    case 'i':
      inform = msa_str_to_format(optarg);
      if (inform == -1) die("Bad argument to -i.\n");
      break;
    case 'M':
      die("-M not yet implemented.\n");
      break;
    case 'd':
      bed_output = 1;
      break;
    case 'h':
      usage(argv[0]);
    case 'v':
      verbose = 1;
      break;
    case '?':
      die("Bad argument.  Try '%s -h'.\n", argv[0]);
    }
  }

  if (backgd_hmm == NULL || backgd_mods == NULL || feat_hmm == NULL ||
      feat_mods == NULL) 
    die("ERROR: must specify -B, -b, -F, and -f.  Try '%s -h'.\n", argv[0]);

  if ((winsize == -1 && features == NULL) || 
      (winsize != -1 && features != NULL))
    die("ERROR: must specify -g or -w but not both.  Try '%s -h'.\n", argv[0]);

  if (backgd_hmm->nstates != backgd_nmods) 
    die("ERROR: number of states must equal number of tree models for background.\n");

  if (feat_hmm->nstates != feat_nmods) 
    die("ERROR: number of states must equal number of tree models for features.\n");

  if (optind != argc - 1) 
    die("ERROR: too few arguments.  Try '%s -h'.\n", argv[0]);

  if (verbose) fprintf(stderr, "Reading alignment ...\n");
  msa = msa_new_from_file(fopen_fname(argv[optind], "r"), inform, NULL);

  /* need ordered representation of alignment */
  if (msa->seqs == NULL && (msa->ss == NULL || msa->ss->tuple_idx == NULL) )
    die("ERROR: ordered sufficient statistics are required.\n");

  /* first have to subtract offset from features, if necessary */
  if (msa->idx_offset != 0 && features != NULL) {
    for (i = 0; i < lst_size(features->features); i++) {
      GFF_Feature *f = lst_get_ptr(features->features, i);
      f->start -= msa->idx_offset;
      f->end -= msa->idx_offset;
    }
  }

  /* convert to coord frame of alignment */
  if (features != NULL) {
    if (verbose) fprintf(stderr, "Mapping coordinates ...\n");
    msa_map_gff_coords(msa, features, 1, 0, 0, NULL); 
  }

  /* Make a reverse complemented copy of the alignment.  The two
     strands will be processed separately, to avoid problems with
     overlapping features, etc. */
  if (verbose) fprintf(stderr, "Creating reverse complemented alignment ...\n");
  msa_compl = msa_create_copy(msa, 0);
  /* temporary workaround: make sure reverse complement not based on
     sufficient stats */
  if (msa_compl->seqs == NULL) ss_to_msa(msa_compl);
  if (msa_compl->ss != NULL) {
    ss_free(msa_compl->ss);
    msa_compl->ss = NULL;
  }
  msa_reverse_compl(msa_compl);

  /* allocate memory for computing scores */
  backgd_emissions = smalloc(backgd_nmods * sizeof(void*));
  for (i = 0; i < backgd_nmods; i++) 
    backgd_emissions[i] = smalloc(msa->length * sizeof(double));
  feat_emissions = smalloc(feat_nmods * sizeof(void*));
  for (i = 0; i < feat_nmods; i++) 
    feat_emissions[i] = smalloc(msa->length * sizeof(double));
  max_nmods = max(backgd_nmods, feat_nmods);
  dummy_emissions = smalloc(max_nmods * sizeof(void*));
  mem = smalloc(max_nmods * sizeof(void*));
  /* memory for forward algorithm -- each block must be as large as
     the largest feature */
  if (features != NULL) {
    for (i = 0, memblocksize = -1; i < lst_size(features->features); i++) {
      GFF_Feature *f = lst_get_ptr(features->features, i);
      if (f->end - f->start + 1 > memblocksize) 
        memblocksize = f->end - f->start + 1;
    }
  }
  else memblocksize = winsize;
  for (i = 0; i < max_nmods; i++)
    mem[i] = smalloc(memblocksize * sizeof(double));

  if (winsize != -1) {
    winscore_pos = smalloc(msa->length * sizeof(double));
    winscore_neg = smalloc(msa->length * sizeof(double));
    for (i = 0; i < msa->length; i++) 
      winscore_pos[i] = winscore_neg[i] = NEGINFTY; 
    no_alignment = smalloc(msa->length * sizeof(int));
    msa_find_noaln(msa, 1, MIN_BLOCK_SIZE, no_alignment);
  }

  /* the rest will be repeated for each strand */
  for (strand = 1; strand <= 2; strand++) {
    MSA *thismsa = strand == 1 ? msa : msa_compl;
    double *winscore = strand == 1 ? winscore_pos : winscore_neg;

    if (verbose) fprintf(stderr, "Processing %c strand ...\n",
                         strand == 1 ? '+' : '-');

    /* set up dummy categories array, so that emissions are only
       computed where needed */
    thismsa->categories = smalloc(thismsa->length * sizeof(int));
    thismsa->ncats = 1;
    if (features == NULL) {
      if (strand == 1)
        for (i = 0; i < thismsa->length; i++) 
          thismsa->categories[i] = no_alignment[i] ? 0 : 1;
      else
        for (i = 0; i < thismsa->length; i++) 
          thismsa->categories[i] = no_alignment[thismsa->length - i - 1] ? 0 : 1;
    }
    else {
      for (i = 0; i < thismsa->length; i++) thismsa->categories[i] = 0;
      for (i = 0; i < lst_size(features->features); i++) {
        GFF_Feature *f = lst_get_ptr(features->features, i);
        if (f->start <= 0 || f->end <= 0) {
          fprintf(stderr, "WARNING: feature out of range ('");
          gff_print_feat(stderr, f);
          fprintf(stderr, "')\n");
          continue;
        }

        if (strand == 1 && f->strand != '-') 
          for (j = f->start - 1; j < f->end; j++)
            thismsa->categories[j] = 1;
        else if (strand == 2 && f->strand == '-')
          for (j = thismsa->length - f->end; 
               j < thismsa->length - f->start + 1; j++)
            thismsa->categories[j] = 1;
      }
    }
    if (thismsa->ss != NULL) ss_update_categories(thismsa);

    /* compute emissions */
    for (i = 0; i < backgd_nmods; i++) {
      if (verbose) 
        fprintf(stderr, "Computing emissions for background model #%d ...\n", i+1);
      tl_compute_log_likelihood(backgd_mods[i], thismsa, 
                                backgd_emissions[i], 1, NULL);
    }
    for (i = 0; i < feat_nmods; i++) {
      if (verbose) 
        fprintf(stderr, "Computing emissions for features model #%d ...\n", i+1);
      tl_compute_log_likelihood(feat_mods[i], thismsa, 
                                feat_emissions[i], 1, NULL);
    }

    /* now compute scores */
    if (winsize != -1) {        /* windows case */
      int winstart;
      if (verbose) fprintf(stderr, "Computing scores ...\n");

      for (winstart = 0; winstart <= thismsa->length - winsize; winstart++) {
        int centeridx = winstart + winsize/2;

        if (strand == 2) centeridx = thismsa->length - centeridx - 1;

        if (no_alignment[centeridx]) continue;

        for (j = 0; j < feat_nmods; j++)
          dummy_emissions[j] = &(feat_emissions[j][winstart]);
        winscore[centeridx] = hmm_forward(feat_hmm, dummy_emissions, 
                                          winsize, mem);

        if (winscore[centeridx] <= NEGINFTY) {
          winscore[centeridx] = NEGINFTY;
          continue;
        }

        for (j = 0; j < backgd_nmods; j++)
          dummy_emissions[j] = &(backgd_emissions[j][winstart]);
        winscore[centeridx] -= hmm_forward(backgd_hmm, dummy_emissions, 
                                           winsize, mem);

        if (winscore[centeridx] < NEGINFTY) winscore[centeridx] = NEGINFTY;
      }
    }
    else {                      /* features case */
      if (verbose) fprintf(stderr, "Computing scores ...\n");
      for (i = 0; i < lst_size(features->features); i++) {
        GFF_Feature *f = lst_get_ptr(features->features, i);
        int s, e;

        if ((strand == 1 && f->strand == '-') || 
            (strand == 2 && f->strand != '-') ||
            f->start <= 0 || f->end <= 0)
          continue;
        
        /* effective coords */
        if (f->strand == '-') {   
          s = thismsa->length - f->end + 1;
          e = thismsa->length - f->start + 1;
        }
        else { s = f->start; e = f->end; }
        
        f->score_is_null = 0;
        
        for (j = 0; j < feat_nmods; j++)
          dummy_emissions[j] = &(feat_emissions[j][s-1]);
        f->score = hmm_forward(feat_hmm, dummy_emissions, e - s + 1, mem);
        
        if (f->score <= NEGINFTY) {
          f->score = NEGINFTY;
          continue;
        }
        
        for (j = 0; j < backgd_nmods; j++)
          dummy_emissions[j] = &(backgd_emissions[j][s-1]);
        f->score -= hmm_forward(backgd_hmm, dummy_emissions, e - s + 1, mem);

        if (f->score < NEGINFTY) f->score = NEGINFTY;
      }
    }
  }

  if (verbose) fprintf(stderr, "Generating output ...\n");
  
  if (winsize != -1) {          /* windows output */
    for (i = 0, j = 0; i < msa->length; i++) {
      if (no_alignment[i] == 0)
        printf("%d\t%.3f\t%.3f\n", j + msa->idx_offset + 1, winscore_pos[i], 
               winscore_neg[i]);
      if (ss_get_char_pos(msa, i, 0, 0) != GAP_CHAR) j++;
    }
  }
  else {                        /* features output */
    /* return to coord frame of reference seq (also, replace offset) */
    msa_map_gff_coords(msa, features, 0, 1, msa->idx_offset, NULL); 

    if (bed_output) 
      gff_print_bed(stdout, features, NULL, NULL);
    else
      gff_print_set(stdout, features);
  }

  if (verbose) fprintf(stderr, "\nDone.\n");

  return 0;
}