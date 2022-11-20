
if(BUILD_MAPTOOL)
   include_directories(${CMAKE_CURRENT_SOURCE_DIR})
   SET(MAPTOOL_SOURCE boundaries.c buffer.c ch.c coastline.c itembin.c itembin_buffer.c misc.c osm.c osm_o5m.c osm_relations.c sourcesink.c tempfile.c tile.c zip.c osm_xml.c)
   SET(MAPTOOL_SOURCE ${MAPTOOL_SOURCE} osm_protobuf.c osm_protobufdb.c generated-code/fileformat.pb-c.c generated-code/osmformat.pb-c.c google/protobuf-c/protobuf-c.c)
   add_library (maptool_core ${MAPTOOL_SOURCE})
   add_executable (maptool maptool.c)
   target_link_libraries(maptool maptool_core ${NAVIT_LIBNAME})

   install(TARGETS maptool
           DESTINATION ${BIN_DIR}
           PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)

endif()
