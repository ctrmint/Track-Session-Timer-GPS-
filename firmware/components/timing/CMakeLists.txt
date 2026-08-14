idf_component_register(
    SRCS "component.cpp" "crossing_validation.cpp" "intersection.cpp"
    INCLUDE_DIRS "include"
    REQUIRES domain track
)
