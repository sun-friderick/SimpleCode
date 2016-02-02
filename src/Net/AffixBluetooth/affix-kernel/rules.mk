# Stuff to automatically maintain dependency files

clean-files := .*o.cmd *.ko *.o *.mod.*

obj-m += $(MODULES-y)
KMODULES := $(MODULES-y:%.o=%.ko)

install: $(KMODULES)
	@install -m 0755 -d $(INSTDIR)
	@install -m 0644 $(KMODULES) $(INSTDIR)

uninstall:
	@cd $(INSTDIR); rm -f $(KMODULES); cd -

clean:
	@rm -f core core.* *.o .*.o *.s *.a *.e tmp_make *~
	@rm -f *.ko .*o.cmd *.mod.*
	@rm -rf .depfiles

