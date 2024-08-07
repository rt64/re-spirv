# re-spirv
re-spirv is a SPIR-V optimizer designed to be as lightweight and fast as possible. It's primarily designed for eliminating dead code after patching spec constants into actual constants, which is also functionality that re-spirv provides. It was originally built for use in [RT64](https://github.com/rt64/rt64), with our intention being that it can become more general-purpose over time.

The primary use case for re-spirv is ubershaders. Applications that use ubershaders need to quickly generate specialized shaders for each combination of spec constants to improve performance. While GPU drivers are also able to perform these optimizations when using shaders with spec constants, re-spirv operates much more quickly (over 100x in some cases) which drastically speeds up pipeline creation.

## Working principles
re-spirv's operation is split into two steps: analysis and optimization. Optimization is done once per output shader, while analysis only needs to be done once per input shader. This is useful when specializing an input shader into many output shaders by patching spec constants.

#### Analysis
The analysis step parses the SPIR-V input and builds several data structures from the instructions and blocks that are present in the binary. The main data structure is a DAG of the instructions with edges representing relationships between instructions. The edges created include: instructions to their result type, instructions with operands to the instructions that produce those operands, branch instructions to the labels for the block(s) they branch to, and OpPhi instructions to the labels of the blocks they reference. This DAG is then topologically sorted in order to create a linear order that instructions can be processed during optimization. 

#### Optimization
The optimization step uses the data structures built during analysis to perform constant propagation, dead code elimination, and dead branch elimination, all in a single incredibly quick pass.

It does this by traversing the sorted DAG that was produced during analysis. During traversal, re-spirv checks if each instruction has only constant operands. For any instructions that do, re-spirv calculates the result using the instruction's formula and stores the result. The result is then marked as constant for future instructions that reference it.

When re-spirv encounters a conditional branch or switch instruction which has a constant-evaluable input, the branch is compacted based on the result of that constant evaluation. Conditional branches are converted into unconditional branches, while switches are reduced to having a single case (to meet the SPIR-V structured control flow requirements), and the indegrees of the labels for the blocks that are no longer referenced by the branch are reduced accordingly.

If a label's indegree reaches zero after processing a branch instruction, the label and any instructions in the block are deleted. Operands referenced by the deleted instructions have their outdegrees reduced, and if any reach zero those instructions are deleted as well. This deletion process propagates backwards in the adjacency list.

## Comparisons with other solutions
There are two other main solutions to the problem that re-spirv solves: using spirv-opt to perform the spec constant patching and optimization, or simply allowing the driver to do the optimizations itself.

#### spirv-opt
spirv-opt is a general-purpose SPIR-V optimizer included in [SPIRV-Tools](https://github.com/KhronosGroup/SPIRV-Tools). It offers many more types of optimizations than what re-spirv has, but in comparison is much slower and a much larger footprint. For comparison, using spirv-opt adds at least 400 source files to a project, while re-spirv is a single source file with fewer than 2000 lines of C++ and a header.

For the 940kB ubershader used by RT64 and a specific set of spec constants to specialize it with, re-spirv produces a 15252 byte SPIR-V binary in 4 milliseconds, 3 of which are the reusable analysis step. Running spirv-opt with the same inputs produces an 11920 byte SPIR-V binary, but takes approximately 600 milliseconds to do so on the same machine. This represents a 600x speed uplift for re-spirv if the analysis is done ahead of time, with a resulting shader that's only 28% larger.

#### Allowing the driver to optimize
In every case we've tested, using re-spirv to optimize the SPIR-V binary before passing it to the driver results in a massive reduction in pipeline creation times when the input misses the driver's shader cache, even when including the time spent optimizing by re-spirv. 

On an Windows system with an NVIDIA GPU, an RT64 scene that took 1.83 seconds to build all pipelines when passing the SPIR-V binaries and spec constants directly to the driver only took 0.25 seconds when using re-spirv to specialize and optimize the shaders. This represents an decrease in pipeline creation times of over 80%.

On a Steam Deck, a scene that took 288 seconds to build all pipelines when passing the SPIR-V binaries and spec constants directly to the driver took only 0.43 seconds when using re-spirv. This represents a decrease in pipeline creation times of over 99.8%.

## Future work

So far, re-spirv has mainly been targeted at optimizing one specific set of shaders (the ubershaders used by RT64). Therefore, there are many SPIR-V instructions that haven't been implemented yet in parsing, analysis, and constant propagation.

One feature that would allow re-spirv to optimize shaders even further is block merging. This would involve merging blocks that are only jump targets for one other block into the block that jumps to them. Special care may need to be taken to ensure that the structured control flow requirements of SPIR-V are still met when doing this.

Another area where re-spirv could gain some ground is in its constant propagation behavior. Currently, the constant propagation algorithm is only able to remove instructions if the propagation results in a branch being compacted. If re-spirv were able to emit new constants into the SPIR-V binary and replace references to instruction results with those constants, it would allow constant propagation to have an effect in cases that aren't branches, such as propagating into the operands of arithmetic or load/store instructions.

Lastly, re-spirv is currently only able to handle 32-bit integer operations during constant propagation, as it was designed for optimizing out branches based on a set of flags. Adding support for other types would allow it to optimize shaders for more use cases.

If you're interested in helping with re-spirv's development, take a look at the [issues page](https://github.com/rt64/re-spirv/issues) to see areas where it can be improved.
