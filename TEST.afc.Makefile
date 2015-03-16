##===- TEST.afc.Makefile ------------------------------*- Makefile -*-===##
#
# Usage: 
#     make TEST=afc (detailed list with time passes, etc.)
#     make TEST=afc report
#     make TEST=afc report.html
#
##===----------------------------------------------------------------------===##

CURDIR  := $(shell cd .; pwd)
PROGDIR := $(PROJ_SRC_ROOT)
RELDIR  := $(subst $(PROGDIR),,$(CURDIR))

LLVM_BUILD = "/home/vhscampos/Research/restritification/llvm/build/Debug+Asserts"


$(PROGRAMS_TO_TEST:%=test.$(TEST).%): \
test.$(TEST).%: Output/%.$(TEST).report.txt
	@cat $<

$(PROGRAMS_TO_TEST:%=Output/%.$(TEST).report.txt):  \
Output/%.$(TEST).report.txt: Output/%.linked.rbc $(LOPT) \
	$(PROJ_SRC_ROOT)/TEST.afc.Makefile 
	$(VERB) $(RM) -f $@
	@echo "---------------------------------------------------------------" >> $@
	@echo ">>> ========= '$(RELDIR)/$*' Program" >> $@
	@echo "---------------------------------------------------------------" >> $@
	@$(LOPT) -mem2reg -load $(LLVM_BUILD)/lib/AliasFunctionCloning.so -tbaa \
		-basicaa -scev-aa -globalsmodref-aa -libcall-aa -scoped-noalias \
		-cfl-aa -afc -O2 -disable-inlining -disable-output -stats -time-passes \
		$< 2>>$@


REPORT_DEPENDENCIES := $(LOPT)
