##
## ZESTO PIN TRACE GENERATOR
##

#TARGET_COMPILER?=gnu
TARGET=ia32
include ../makefile.gnu.config
#CXXFLAGS ?= -Wall -Werror -Wno-unknown-pragmas $(DBG) $(OPT)

##############################################################
#
# Tools sets
#
##############################################################

TOOL_ROOTS = itrace

TOOLS = $(TOOL_ROOTS:%=$(OBJDIR)%$(PINTOOL_SUFFIX))

##############################################################
#
# build rules
#
##############################################################
all: $(OBJDIR)
	-$(MAKE) make_all
tools: $(OBJDIR)
	-$(MAKE) make_tools

make_all: make_tools
make_tools: $(TOOLS)

##############################################################
#
# build rules
#
##############################################################

$(OBJDIR)make-directory:
	mkdir -p $(OBJDIR)
	touch $(OBJDIR)make-directory
$(OBJDIR):
	mkdir -p $(OBJDIR)

$(OBJDIR)%.o : %.cpp $(OBJDIR)make-directory
	$(CXX) -c -m32 $(CXXFLAGS) $(PIN_CXXFLAGS) ${OUTOPT}$@ $<

$(TOOLS): $(PIN_LIBNAMES)

$(TOOLS): %$(PINTOOL_SUFFIX) : %.o
	${PIN_LD} $(PIN_LDFLAGS) $(LINK_DEBUG) ${LINK_OUT}$@ $< ${PIN_LPATHS} $(PIN_LIBS) $(DBG)

## cleaning
clean:
	-rm -rf $(OBJDIR) *.out *.tested *.failed *.makefile.copy *.out.*.* *.log *~
