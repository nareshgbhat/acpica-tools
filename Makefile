#
# Common make for acpica tools and utilities
#

#
# Note: This makefile is intended to be used from within the native
# ACPICA directory structure, from under top level acpica directory.
# It specifically places all the object files for each tool in separate
# generate/unix subdirectories, not within the various ACPICA source
# code directories. This prevents collisions between different
# compilations of the same source file with different compile options.
#
BUILD_DIRECTORY_PATH = "generate/unix"

include generate/unix/Makefile.config
include generate/unix/Makefile.common

check:
	cd tests

	# ASL tests
	$(CURDIR)/tests/aslts.sh $(CURDIR)/tests/aslts $(CURDIR)/generate/unix

	# API Tests
	$(CURDIR)/debian/run-aapits.sh $(CURDIR)/tests/aapits $(CURDIR)/generate/unix/bin

	# misc tests
	$(CURDIR)/debian/run-misc-tests.sh $(CURDIR) 20130626

	# Template tests
	cd $(CURDIR)/tests/templates
	make
	if [ -f diff.log ] ; \
	then \
		if [ -s diff.log ] ; \
		then \
			exit 1		# implies errors occurred ; \
		fi ; \
	fi

