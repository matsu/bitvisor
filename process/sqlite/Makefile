CFLAGS += -Iprocess/lib

CONSTANTS-y += -DSQLITE_OMIT_LOAD_EXTENSION
CONSTANTS-y += -DSQLITE_THREADSAFE=0
CONSTANTS-y += -DSQLITE_OMIT_LOCALTIME
CONSTANTS-y += -DSQLITE_OMIT_FLOATING_POINT
CONSTANTS-y += -DSQLITE_OS_OTHER
CONSTANTS-y += -DSQLITE_ENABLE_MEMSYS5
CONSTANTS-y += -DSQLITE_ZERO_MALLOC
CONSTANTS-y += -DSQLITE_OMIT_AUTOINIT

sqliteexample-objs = sqliteexample.o vvfs.o sqlite3.o
sqliteexample-libs = process/lib/$(outa)
sqliteexample-stacksize = 16384

bins-y += sqliteexample

# The text size of sqlite3.o will be more than 500KB.  If another
# program is added and the sqliteexample is not needed, use = like
# bins-y=program to avoid linking the sqliteexample and reduce the
# binary size.
