The opening book has to be converted to the include file format.

ATTENTION! the file names are important and have to stay as they are
(unless you include the necessary changes in book.c). Otherwise, you will
get linker errors because of unresolved symbols.

Edit the opening book file "bookdata.txt" as you like.


Windows:
Run the book tool which converts the line-based opening format into a
position-based binary format so that transpositions will be recognised:
booktool_win.bat
(Note: MingW was used to generate the precompiled Windows binary, see
make_booktool.bat)

Linux or Cygwin:
first, generate an executable book tool: ./make_booktool.sh
Run the book tool which converts the line-based opening format into a
position-based binary format so that transpositions will be recognised:
./run_booktool.sh


(Fix possible erroneous lines in bookdata.txt, or the converted
opening book file will NOT be generated!)

This will generate bookdata.c, the include file. Copy bookdata.c into
the application folder where book.c is located and re-build the binary.