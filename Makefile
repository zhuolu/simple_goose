
U_TARGET = gs_tran gs_recv

obj-m := goose.o

SRC_PATH := src
USRC_PATH := usrc
JADE_PATH := jade
goose-objs := $(SRC_PATH)/goose_main.o

CURRENT = $(shell uname -r)
KDIR = /lib/modules/$(CURRENT)/build
PWD = $(shell pwd)

export PWD

.PONY: default, clean

default:
	@make -C $(KDIR) M=$(PWD) modules;\
	rm -rf *.o *.mod.c *.symvers .*.ko.cmd .*.o.cmd $(SRC_PATH)/*.o $(SRC_PATH)/.*.o.cmd;\
	rm -rf Module.markers modules.order .tmp_versions;
	@make -C $(USRC_PATH);
	@make -C $(JADE_PATH);

clean: 
	@rm -rf *.ko *.o *.mod.c *.symvers .*.ko.cmd .*.o.cmd $(SRC_PATH)/*.o $(SRC_PATH)/.*.o.cmd $(SRC_PATH)/.*.o.d;
	@rm -rf Module.markers modules.order .tmp_versions $(U_TARGET)
	@make clean -C $(USRC_PATH);
