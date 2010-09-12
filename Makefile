TMPDIR = /tmp/phast
CWD = ${PWD}

all:
	@echo "Type \"make package\" to create a tarball reflecting the current state of the CVS tree."

package:
	rm -rf ${TMPDIR}
	mkdir -p ${TMPDIR}
	cd ${TMPDIR} ; svn checkout http://compgen.bscb.cornell.edu/svnrepo/phast/trunk phast
	find ${TMPDIR}/phast -name ".svn" | xargs rm -rf
	rm -r ${TMPDIR}/phast/doc ${TMPDIR}/phast/src/lib/rphast ${TMPDIR}/phast/test
	VERSION=`cat ${TMPDIR}/phast/version | sed 's/\./_/g'` ;\
	cd ${TMPDIR} ; tar cfz ${CWD}/phast.$$VERSION.tgz phast
	rm -rf ${TMPDIR}
