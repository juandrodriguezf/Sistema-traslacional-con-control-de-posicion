include("${CMAKE_CURRENT_LIST_DIR}/rule.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/file.cmake")

set(nema_pcb_default_library_list )

# Handle files with suffix (s|as|asm|AS|ASM|As|aS|Asm), for group default-XC8
if(nema_pcb_default_default_XC8_FILE_TYPE_assemble)
add_library(nema_pcb_default_default_XC8_assemble OBJECT ${nema_pcb_default_default_XC8_FILE_TYPE_assemble})
    nema_pcb_default_default_XC8_assemble_rule(nema_pcb_default_default_XC8_assemble)
    list(APPEND nema_pcb_default_library_list "$<TARGET_OBJECTS:nema_pcb_default_default_XC8_assemble>")

endif()

# Handle files with suffix S, for group default-XC8
if(nema_pcb_default_default_XC8_FILE_TYPE_assemblePreprocess)
add_library(nema_pcb_default_default_XC8_assemblePreprocess OBJECT ${nema_pcb_default_default_XC8_FILE_TYPE_assemblePreprocess})
    nema_pcb_default_default_XC8_assemblePreprocess_rule(nema_pcb_default_default_XC8_assemblePreprocess)
    list(APPEND nema_pcb_default_library_list "$<TARGET_OBJECTS:nema_pcb_default_default_XC8_assemblePreprocess>")

endif()

# Handle files with suffix [cC], for group default-XC8
if(nema_pcb_default_default_XC8_FILE_TYPE_compile)
add_library(nema_pcb_default_default_XC8_compile OBJECT ${nema_pcb_default_default_XC8_FILE_TYPE_compile})
    nema_pcb_default_default_XC8_compile_rule(nema_pcb_default_default_XC8_compile)
    list(APPEND nema_pcb_default_library_list "$<TARGET_OBJECTS:nema_pcb_default_default_XC8_compile>")

endif()


# Main target for this project
add_executable(nema_pcb_default_image_HNlUBf_4 ${nema_pcb_default_library_list})

set_target_properties(nema_pcb_default_image_HNlUBf_4 PROPERTIES
    OUTPUT_NAME "default"
    SUFFIX ".elf"
    ADDITIONAL_CLEAN_FILES "${output_extensions}"
    RUNTIME_OUTPUT_DIRECTORY "${nema_pcb_default_output_dir}")
target_link_libraries(nema_pcb_default_image_HNlUBf_4 PRIVATE ${nema_pcb_default_default_XC8_FILE_TYPE_link})

# Add the link options from the rule file.
nema_pcb_default_link_rule( nema_pcb_default_image_HNlUBf_4)


