add_custom_target(RewriterTests)
set_target_properties(RewriterTests PROPERTIES FOLDER "RewriterTests")

add_unittest(RewriterTests RewriterTest ConstraintTests ConstraintTests.cpp ../Constraints.cpp)
