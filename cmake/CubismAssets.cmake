function(bongo_cat_configure_embedded_cubism_assets)
  set(BONGO_CAT_ASSET_EXTRA_COMMANDS PARENT_SCOPE)
  if(NOT BONGO_CAT_CUBISM_ENABLED)
    return()
  endif()
  file(GLOB_RECURSE shader_inputs CONFIGURE_DEPENDS
    "${CUBISM_FRAMEWORK_PATH}/src/Rendering/OpenGL/Shaders/Standard/*")
  set(BONGO_CAT_ASSET_INPUTS ${BONGO_CAT_ASSET_INPUTS} ${shader_inputs} PARENT_SCOPE)
  set(BONGO_CAT_ASSET_EXTRA_COMMANDS
    COMMAND ${CMAKE_COMMAND} -E copy_directory
      "${CUBISM_FRAMEWORK_PATH}/src/Rendering/OpenGL/Shaders/Standard"
      "${BONGO_CAT_ASSET_STAGE}/assets/FrameworkShaders" PARENT_SCOPE)
endfunction()

function(bongo_cat_stage_cubism_assets target)
  if(APPLE)
    add_custom_command(TARGET ${target} POST_BUILD COMMAND ${CMAKE_COMMAND} -E rm -f
      "$<TARGET_FILE_DIR:${target}>/assets/logo-mac.png"
      "$<TARGET_FILE_DIR:${target}>/assets/ui-icons.png"
      "$<TARGET_FILE_DIR:${target}>/assets/ui-symbols-1x.png"
      "$<TARGET_FILE_DIR:${target}>/assets/ui-symbols-4x.png")
  endif()
  if(NOT BONGO_CAT_CUBISM_ENABLED)
    return()
  endif()
  add_custom_command(TARGET ${target} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
      "${BONGO_CAT_CUBISM_SDK}/Framework/src/Rendering/OpenGL/Shaders/Standard"
      "$<TARGET_FILE_DIR:${target}>/FrameworkShaders")
  if(UNIX AND NOT APPLE)
    install(DIRECTORY
      "${CUBISM_FRAMEWORK_PATH}/src/Rendering/OpenGL/Shaders/Standard/"
      DESTINATION assets/FrameworkShaders)
  endif()
endfunction()
