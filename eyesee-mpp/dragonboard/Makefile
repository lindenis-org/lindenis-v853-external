############################################################################################
# 			eyesee-mpp-dragonboard for tina(OpenWrt) Linux
#
#	eyesee-mpp is designed for CDR/SDV product, focus on video/audio capturing and encoding, 
# it also can support video/audio decode.
#   eyesee-mpp-custom is our solutions to some products such as sdv. It mainly uses 
# to implement multimedia function.
#
# Version: v1.0
# Date   : 2019-2-21
# Author : PDC-PD5
############################################################################################
all:
	@echo ==================================================
	@echo build eyesee-mpp-dragonboard
	@echo ==================================================
	make -C lib                                    all
	make -C apps/DragonBoard/config                all
	make -C apps/DragonBoard/res                   all
	make -C apps/DragonBoard/core                  all
	make -C apps/DragonBoard/testcase/rtctester    all
	make -C apps/DragonBoard/testcase/ledtester    all
	make -C apps/DragonBoard/testcase/usbtester    all
	make -C apps/DragonBoard/testcase/ddrtester    all
	make -C apps/DragonBoard/testcase/tftester     all
	make -C apps/DragonBoard/testcase/nortester    all
	make -C apps/DragonBoard/testcase/nandtester   all
	make -C apps/DragonBoard/testcase/csitester    all
	make -C apps/DragonBoard/testcase/mictester    all
	make -C apps/DragonBoard/testcase/venctester   all
	make -C apps/DragonBoard/testcase/vdectester   all
	make -C apps/DragonBoard/testcase/isptester    all
	make -C apps/DragonBoard/testcase/g2dtester    all
	make -C apps/DragonBoard/testcase/cetester     all
	make -C apps/DragonBoard/testcase/votester     all
	@echo build eyesee-mpp-dragonboard done!

clean:
	@echo ==================================================
	@echo clean eyesee-mpp-dragonboard
	@echo ==================================================
	make -C lib                                    clean
	make -C apps/DragonBoard/config                clean
	make -C apps/DragonBoard/res                   clean
	make -C apps/DragonBoard/core                  clean
	make -C apps/DragonBoard/testcase/rtctester    clean
	make -C apps/DragonBoard/testcase/ledtester    clean
	make -C apps/DragonBoard/testcase/usbtester    clean
	make -C apps/DragonBoard/testcase/ddrtester    clean
	make -C apps/DragonBoard/testcase/tftester     clean
	make -C apps/DragonBoard/testcase/nortester    clean
	make -C apps/DragonBoard/testcase/nandtester   clean
	make -C apps/DragonBoard/testcase/csitester    clean
	make -C apps/DragonBoard/testcase/mictester    clean
	make -C apps/DragonBoard/testcase/venctester   clean
	make -C apps/DragonBoard/testcase/vdectester   clean
	make -C apps/DragonBoard/testcase/isptester    clean
	make -C apps/DragonBoard/testcase/g2dtester    clean
	make -C apps/DragonBoard/testcase/cetester     clean
	make -C apps/DragonBoard/testcase/votester     clean
	@echo clean eyesee-mpp-dragonboard done!
