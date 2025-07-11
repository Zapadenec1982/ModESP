idf_component_register(SRCS "test_memory_pool.cpp"
                    INCLUDE_DIRS "."
                    # Додайте тут залежність від вашого core компонента,
                    # де знаходяться memory_pool та memory_diagnostics
                    REQUIRES unity core
)