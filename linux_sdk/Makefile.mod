#
# wrapper Makefile for auto-generated make files
#
#

#############################################################################
# PROJECT MAKEFILES
#############################################################################
MAKE_FILE=Makefile.$(MOD_CONFIG)
include $(MAKE_FILE)

#############################################################################
# The compiler command lne for each src code file to compile
#############################################################################
DO_CC=$(CPLUS) -w $(INCLUDES) $(CFLAGS) -o $@ -c $<


clean:
	rm -rf obj/$(NAME)
	rm -f $(NAME)_$(ARCH).$(SHLIBEXT)
