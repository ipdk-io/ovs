if P4OVS
ovsp4rtincludedir = $(includedir)/ovsp4rt
ovsp4rtinclude_HEADERS = \
	include/ovsp4rt/ovs-p4rt.h

install-data-hook:
	cd $(DESTDIR)$(includedir)/openvswitch && \
	    $(LN_S) ../ovsp4rt/ovs-p4rt.h ovs-p4rt.h
endif
