#
# Find Embree
#
# Try to find Embree.
# This module defines the following variables:
# - EMBREE_INCLUDE_DIRS
# - EMBREE_LIBRARIES
# - EMBREE_FOUND
#
# The following variables can be set as arguments for the module.
# - EMBREE_ROOT_DIR : Root library directory of Embree
#

#
#  Lightmetrica - A modern, research-oriented renderer
# 
#  Copyright (c) 2015 Hisanari Otsu
#  
#  Permission is hereby granted, free of charge, to any person obtaining a copy
#  of this software and associated documentation files (the "Software"), to deal
#  in the Software without restriction, including without limitation the rights
#  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
#  copies of the Software, and to permit persons to whom the Software is
#  furnished to do so, subject to the following conditions:
#  
#  The above copyright notice and this permission notice shall be included in
#  all copies or substantial portions of the Software.
#  
#  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
#  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
#  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
#  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
#  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
#  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
#  THE SOFTWARE.
#

# Additional modules
include(FindPackageHandleStandardArgs)

if (WIN32)
	# Find include files
	find_path(
		EMBREE_INCLUDE_DIR
		NAMES embree2/rtcore.h
		PATHS
			$ENV{PROGRAMFILES}/include
			${GLEW_ROOT_DIR}/include
		DOC "The directory where embree2/rtcore.h resides")

	# Find library files
	find_library(
		EMBREE_LIBRARY_RELEASE
		NAMES embree
		PATHS
			$ENV{PROGRAMFILES}/lib
			${EMBREE_ROOT_DIR}/lib
			${LM_EXTERNAL_LIBRARY_PATH}/Release)

	find_library(
		EMBREE_LIBRARY_DEBUG
		NAMES embree
		PATHS
			$ENV{PROGRAMFILES}/lib
			${EMBREE_ROOT_DIR}/lib
			${LM_EXTERNAL_LIBRARY_PATH}/Debug)
else()
	# Find include files
	find_path(
		EMBREE_INCLUDE_DIR
		NAMES embree2/rtcore.h
		PATHS
			/usr/include
			/usr/local/include
			/sw/include
			/opt/local/include
		DOC "The directory where embree2/rtcore.h resides")

	# Find library files
	find_library(
		EMBREE_LIBRARY
		NAMES embree embree.2
		PATHS
			/usr/lib64
			/usr/lib
			/usr/local/lib64
			/usr/local/lib
			/sw/lib
			/opt/local/lib
			${EMBREE_ROOT_DIR}/lib
		DOC "The Embree library")
endif()

if (WIN32)
	# Handle REQUIRD argument, define *_FOUND variable
	find_package_handle_standard_args(Embree DEFAULT_MSG EMBREE_INCLUDE_DIR EMBREE_LIBRARY_RELEASE EMBREE_LIBRARY_DEBUG)

	# Define EMBREE_LIBRARIES and EMBREE_INCLUDE_DIRS
	if (EMBREE_FOUND)
		set(EMBREE_LIBRARIES_RELEASE ${EMBREE_LIBRARY_RELEASE})
		set(EMBREE_LIBRARIES_DEBUG ${EMBREE_LIBRARY_DEBUG})
		set(EMBREE_LIBRARIES debug ${EMBREE_LIBRARIES_DEBUG} optimized ${EMBREE_LIBRARY_RELEASE})
		set(EMBREE_INCLUDE_DIRS ${EMBREE_INCLUDE_DIR})
	endif()

	# Hide some variables
	mark_as_advanced(EMBREE_INCLUDE_DIR EMBREE_LIBRARY_RELEASE EMBREE_LIBRARY_DEBUG)
else()
	find_package_handle_standard_args(Embree DEFAULT_MSG EMBREE_INCLUDE_DIR EMBREE_LIBRARY)

	if (EMBREE_FOUND)
		set(EMBREE_LIBRARIES ${EMBREE_LIBRARY})
		set(EMBREE_INCLUDE_DIRS ${EMBREE_INCLUDE_DIR})
	endif()

	mark_as_advanced(EMBREE_INCLUDE_DIR EMBREE_LIBRARY)
endif()
