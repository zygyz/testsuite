SUBDIRS = i386-unknown-linux2.4 x86_64-unknown-linux2.4 ppc32_linux ppc64_linux i386-unknown-freebsd7.2 amd64-unknown-freebsd7.2 ppc64_bgq_ion
SUBDIR_WINDOWS = i386-unknown-nt4.0

TO_SPEC = src/specification
PROLOG_FILES = $(TO_SPEC)/spec-grouped.pl $(TO_SPEC)/util.pl $(TO_SPEC)/test.pl
PYTHON_FILES = $(TO_SPEC)/cmake_mutatees.py $(TO_SPEC)/cmake_mutators.py $(TO_SPEC)/generate.py $(TO_SPEC)/group_boilerplate.py $(TO_SPEC)/parse.py $(TO_SPEC)/test_info_new_gen.py $(TO_SPEC)/tuples.py $(TO_SPEC)/utils.py

.PHONY: usage gen-all echo $(SUBDIRS) $(SUBDIR_WINDOWS)

usage:
	@echo "Use target 'gen-all' to regenerate generated files for all supported"
	@echo "platforms"
	@echo "Use target 'gen-clean' to remove generated files for all supported"
	@echo "platforms"
	@echo "Use target PLATFORM to make for a specific platform"

ONE_GENERATED_FILE = cmake-mutatees.txt
ALL_GENERATED_FILES = $(foreach dir,$(SUBDIRS),$(ONE_GENERATED_FILE:%=$(dir)/%))

gen-all: $(ALL_GENERATED_FILES)

gen-clean:
	-rm -f $(ALL_GENERATED_FILES)


$(SUBDIRS:%=%/tuples): %/tuples: $(PROLOG_FILES)
	cd $(TO_SPEC); gprolog --entry-goal "['spec-grouped.pl']" \
		--entry-goal "test_init('$*')" \
		--entry-goal "write_tuples('../../$@', '$*')" \
		--entry-goal "halt"

$(SUBDIRS:%=%/cmake-mutatees.txt): %/cmake-mutatees.txt: $(PYTHON_FILES) %/tuples
	python -c "import sys; import os; os.environ['PLATFORM'] = '$*'; sys.path.append('$(TO_SPEC)'); import generate ; generate.generate('$*')"

$(SUBDIRS:%=%): %:%/cmake-mutatees.txt

