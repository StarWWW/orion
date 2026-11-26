#---------------------------------------------------------------------
# Makefile for Orion (NVIDIA Blackwell sm_120 GPU Bitcoin key scanner)
#
# Default target is sm_120 (Blackwell). Other GPUs: make CCAP=89 (Ada), CCAP=86 (Ampere), ...
# Custom CUDA location: make CUDA=/usr/local/cuda-12.8

SRC = Base58.cpp main.cpp Timer.cpp Int.cpp IntMod.cpp Point.cpp \
      SECP256K1.cpp Orion.cpp hash/ripemd160.cpp hash/sha256.cpp Bech32.cpp

OBJDIR = obj

OBJET = $(addprefix $(OBJDIR)/, \
        Base58.o main.o Timer.o Int.o IntMod.o Point.o \
        SECP256K1.o Orion.o hash/ripemd160.o hash/sha256.o \
        GPU/GPUEngine.o Bech32.o)

.DEFAULT_GOAL := all

CXX        = g++
CUDA       = /usr/local/cuda
CXXCUDA    = $(CXX)
NVCC       = $(CUDA)/bin/nvcc
CCAP       = 120
GENCODE    = -gencode=arch=compute_$(CCAP),code=sm_$(CCAP) -gencode=arch=compute_$(CCAP),code=compute_$(CCAP)

ifdef debug
CXXFLAGS   = -std=c++17 -Wno-write-strings -g -fno-strict-aliasing -I. -I$(CUDA)/include -include cstdint
else
CXXFLAGS   = -std=c++17 -Wno-write-strings -O3 -fno-strict-aliasing -I. -I$(CUDA)/include -include cstdint
endif
LFLAGS     = -lpthread -L$(CUDA)/lib64 -lcudart_static -ldl -lrt

# No generated dependency files: a header change rebuilds every object that may include it
HEADERS     = $(wildcard *.h) $(wildcard hash/*.h) GPU/GPUEngine.h
GPU_HEADERS = $(HEADERS) $(wildcard GPU/*.h)

#--------------------------------------------------------------------

all: orion

ifdef debug
$(OBJDIR)/GPU/GPUEngine.o: GPU/GPUEngine.cu $(GPU_HEADERS)
	$(NVCC) -std=c++17 -G -maxrregcount=0 --ptxas-options=-v --compile --compiler-options -fPIC -ccbin $(CXXCUDA) -m64 -g -I$(CUDA)/include $(GENCODE) -o $(OBJDIR)/GPU/GPUEngine.o -c GPU/GPUEngine.cu
else
$(OBJDIR)/GPU/GPUEngine.o: GPU/GPUEngine.cu $(GPU_HEADERS)
	$(NVCC) -std=c++17 -maxrregcount=0 --ptxas-options=-v --compile --compiler-options -fPIC -ccbin $(CXXCUDA) -m64 -O3 -I$(CUDA)/include $(GENCODE) -o $(OBJDIR)/GPU/GPUEngine.o -c GPU/GPUEngine.cu
endif

$(OBJDIR)/%.o : %.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) -o $@ -c $<

orion: $(OBJET)
	@echo Making Orion...
	$(CXX) $(OBJET) $(LFLAGS) -o $@

$(OBJET): | $(OBJDIR) $(OBJDIR)/GPU $(OBJDIR)/hash

$(OBJDIR):
	mkdir -p $(OBJDIR)

$(OBJDIR)/GPU: $(OBJDIR)
	mkdir -p $(OBJDIR)/GPU

$(OBJDIR)/hash: $(OBJDIR)
	mkdir -p $(OBJDIR)/hash

clean:
	@echo Cleaning...
	@rm -f $(OBJDIR)/*.o
	@rm -f $(OBJDIR)/GPU/*.o
	@rm -f $(OBJDIR)/hash/*.o
	@rm -f orion

.PHONY: all clean
