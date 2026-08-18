idf_component_register(
    SRCS "track_catalog.cpp" "track_loader.cpp"
    INCLUDE_DIRS "include"
    REQUIRES domain settings timing track
)
