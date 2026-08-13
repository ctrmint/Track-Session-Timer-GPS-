idf_component_register(
    SRCS "app_main.cpp"
    INCLUDE_DIRS "."
    REQUIRES domain board gnss timing track session logger ui diagnostics
)
