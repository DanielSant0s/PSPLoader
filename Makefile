EE_BIN = psp_loader.elf
EE_OBJS = main.o
EE_LIBS =  -L$(PS2SDK)/ports/lib -ldebug -lkernel -lpatches -lps2_drivers
EE_INCS +=  -I$(PS2SDK)/ports/include

all: $(EE_BIN)
	$(EE_STRIP) --strip-all $(EE_BIN)

clean:
	rm -f $(EE_BIN) $(EE_OBJS) cube.c

run: $(EE_BIN)
	ps2client execee host:$(EE_BIN)

reset:
	ps2client reset

include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal