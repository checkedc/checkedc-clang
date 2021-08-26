.PHONY: cmake build build-all cool full

# Check SMT prover
cmake:
	mkdir -p build
	cd build && cmake -G Ninja -DLLVM_ENABLE_PROJECTS="clang;clang-tools-extra" \
			-DCMAKE_INSTALL_PREFIX=/home/shiwei/code/install \
			-DCMAKE_BUILD_TYPE=Debug -DLLVM_ENABLE_ASSERTIONS=ON \
			-DLLVM_CCACHE_BUILD=ON -DLLVM_INSTALL_TOOLCHAIN_ONLY=ON \
			-DLLVM_TARGETS_TO_BUILD="X86" -DLLVM_LIT_ARGS=-v \
			-DLLVM_OPTIMIZED_TABLEGEN=ON \
			-DLLVM_USE_LINKER=gold \
			-DLLVM_PARALLEL_LINK_JOBS=48 \
 			-DLLVM_ENABLE_Z3_SOLVER=ON \
			/home/shiwei/code/src/llvm

build:
	cd build && ninja clang

cool: cmake build

p:
	clang -cc1 prover-test.c

# Invoke checkers
checker1:
	clang -cc1 -analyze -analyzer-checker alpha.security.SimpleBounds 1.c

checker2:
	clang -cc1 -analyze -analyzer-constraints=z3 -analyzer-checker alpha.security.ArrayBoundV2 1.c

# Test available facts
test1:
	llvm-lit src/clang/test/CheckedC/program-invariants/available-facts.c
test2:
	llvm-lit src/clang/test/CheckedC/program-invariants/dataflow-facts-imported.c
