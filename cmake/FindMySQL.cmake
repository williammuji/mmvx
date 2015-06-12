FIND_PATH(MYSQL_INCLUDE_DIR mysql.h
	/usr/include/mysql
	/usr/local/include/mysql
	/opt/mysql/mysql/include
	/opt/mysql/mysql/include/mysql
	/opt/mysql/include
	/opt/local/include/mysql5
	/usr/local/mysql/include
	/usr/local/mysql/include/mysql)

FIND_LIBRARY(MYSQL_LIBRARY
	NAMES mysqlclient_r
	PATHS /usr/lib /usr/lib/mysql /usr/local/lib /usr/local/mysql/lib /usr/local/lib/mysql /usr/local/mysql/lib/mysql
	)

IF (MYSQL_INCLUDE_DIR AND MYSQL_LIBRARY)
	SET(MYSQL_FOUND TRUE)

	INCLUDE_DIRECTORIES(${MYSQL_INCLUDE_DIR})
	LINK_DIRECTORIES(${MYSQL_LIBRARY})

	MESSAGE(STATUS "MySQL Include dir: ${MYSQL_INCLUDE_DIR}  library dir: ${MYSQL_LIBRARY}")
ELSEIF (MySQL_FIND_REQUIRED)
	MESSAGE(FATAL_ERROR "Cannot find MySQL. Include dir: ${MYSQL_INCLUDE_DIR}  library dir: ${MYSQL_LIBRARY}")
ENDIF (MYSQL_INCLUDE_DIR AND MYSQL_LIBRARY)

MARK_AS_ADVANCED(
	MYSQL_LIBRARY	
	MYSQL_INCLUDE_DIR
	)
