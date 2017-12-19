set(all_source_files )
set(all_include_files )
set(all_include_directories )

foreach(dependency ${src_source_dependencies})
	getSourceFiles(dep_files "${FRC_ROOT_DIR}/src/${dependency}")
	list(APPEND all_source_files ${dep_files})
endforeach(dependency)

foreach(dependency ${test_source_dependencies})
	getSourceFiles(dep_files "${FRC_ROOT_DIR}/test/${dependency}")
	list(APPEND all_source_files ${dep_files})
endforeach(dependency)

foreach(dependency ${test_include_dependencies})
	getHeaderFiles(dep_files "${FRC_ROOT_DIR}/test/${dependency}")
	list(APPEND all_include_files ${dep_files})
	list(APPEND all_include_directories "${FRC_ROOT_DIR}/test/${dependency}")
endforeach(dependency)

foreach(dependency ${benchmark_source_dependencies})
	getSourceFiles(dep_files "${FRC_ROOT_DIR}/benchmark/${dependency}")
	list(APPEND all_source_files ${dep_files})
endforeach(dependency)

foreach(dependency ${benchmark_include_dependencies})
	getHeaderFiles(dep_files "${FRC_ROOT_DIR}/benchmark/${dependency}")
	list(APPEND all_include_files ${dep_files})
	list(APPEND all_include_directories "${FRC_ROOT_DIR}/benchmark/${dependency}")
endforeach(dependency)

