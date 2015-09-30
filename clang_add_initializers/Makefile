CXX := clang++
LLVMCONFIG := $(shell which llvm-config || echo llvm-config-3.5)

CXXFLAGS := -I$(shell $(LLVMCONFIG) --src-root)/tools/clang/include \
	-I$(shell $(LLVMCONFIG) --obj-root)/tools/clang/include \
	$(shell $(LLVMCONFIG) --cxxflags) \
	-g2 -fno-rtti

SOURCES = CIrewriter.cpp

OBJECTS = $(SOURCES:.cpp=.o)
EXES = $(OBJECTS:.o=)
CLANGLIBS = \
		-lclangAST \
		-lclangASTMatchers \
		-lclangAnalysis \
		-lclangApplyReplacements \
		-lclangBasic \
		-lclangCodeGen \
		-lclangDriver \
		-lclangDynamicASTMatchers \
		-lclangEdit \
		-lclangFormat \
		-lclangFrontend \
		-lclangFrontendTool \
		-lclangIndex \
		-lclangLex \
		-lclangParse \
		-lclangQuery \
		-lclangRename \
		-lclangRewrite \
		-lclangRewriteFrontend \
		-lclangSema \
		-lclangSerialization \
		-lclangTooling \
		-lclangToolingCore

LDFLAGS := \
	-L/opt/my_clang/lib \
	-Wl,--start-group \
	$(shell $(LLVMCONFIG) --ldflags --system-libs --libs all) \
	-lc++ -lc++abi \
	$(CLANGLIBS) \
	-Wl,--end-group

all: $(OBJECTS) $(EXES)

%: %.o
	$(CXX) -o $@ $< $(CXXFLAGS) $(LDFLAGS)

clean:
	-rm -f $(EXES) $(OBJECTS) *~
