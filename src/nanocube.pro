LOCAL=$$system(echo $HOME)/local


QMAKE_CFLAGS_X86_64   -= -mmacosx-version-min=10.5
QMAKE_CXXFLAGS_X86_64 -= -mmacosx-version-min=10.5
QMAKE_LFLAGS_X86_64   -= -mmacosx-version-min=10.5
QMAKE_LFLAGS          -= -mmacosx-version-min=10.5

# $$system(echo /Users/lauro)

macx {
# mac only
# -stdlib=libc++
# -stdlib=libc++
#QMAKE_CXXFLAGS  += -stdlib=libc++
#QMAKE_LFLAGS    += -stdlib=libc++
QMAKE_CXXFLAGS  += -std=c++11 -mmacosx-version-min=10.7 -Wno-unused-parameter -I$${LOCAL}/include
QMAKE_LFLAGS    += -std=c++11 -mmacosx-version-min=10.7 -Wno-unused-parameter
}
unix:!macx{
# linux only
QMAKE_CXXFLAGS  += -std=c++11 -I$${LOCAL}/include -Wunused-parameter
QMAKE_LFLAGS    += -std=c++11
}

QT     -=  gui core
LIBS   -= -lQtGui -lQtCore

LIBS        += -L$${LOCAL}/lib -ldl -lpthread -lz
INCLUDEPATH += .

EXAMPLE1 = \
   -DLIST_DIMENSION_NAMES=c1,c1,c1,c1 \
   -DLIST_VARIABLE_TYPES=u2,u4

EXAMPLE2 = \
   -DLIST_DIMENSION_NAMES=q2,c1 \
   -DLIST_VARIABLE_TYPES=u2,u4

EXAMPLE3 = \
   -DLIST_DIMENSION_NAMES=q2,q2 \
   -DLIST_VARIABLE_TYPES=u2,u4

EXAMPLE4 = \
   -DLIST_DIMENSION_NAMES=q2,q2 \
   -DLIST_VARIABLE_TYPES=u2,u4

EXAMPLE5 = \
   -DLIST_DIMENSION_NAMES=q20,q20 \
   -DLIST_VARIABLE_TYPES=u2,u4

EXAMPLE6 = \
   -DLIST_DIMENSION_NAMES=c2 \
   -DLIST_VARIABLE_TYPES=u2,u4

EXAMPLE7 = \
   -DLIST_DIMENSION_NAMES=c1,c1,c1 \
   -DLIST_VARIABLE_TYPES=u1,u1

EXAMPLE8 = \
   -DLIST_DIMENSION_NAMES=q1,q1,q1 \
   -DLIST_VARIABLE_TYPES=u1,u1

EXAMPLE9 = \
   -DLIST_DIMENSION_NAMES=q25,c2,c1 \
   -DLIST_VARIABLE_TYPES=u2,u4

# example 1
QMAKE_CXXFLAGS  += -DLOCALRUN -DxFLATTREE_VECTOR -DxTIMESERIES_VECTOR -D_GLIBCXX_USE_NANOSLEEP \
   -DSUPER_FAST_COMPILE -DCOLLECT_MEMUSAGE \
   $${EXAMPLE7}

HEADERS =                 \
MercatorProjection.hh     \
QuadTree.hh               \
QuadTreeNode.hh           \
QuadTreeUtil.hh           \
Stopwatch.hh              \
TimeSeries.hh             \
Common.hh                 \
FlatTree.hh               \
FlatTreeN.hh              \
ContentHolder.hh          \
small_vector.hh           \
TaggedPointer.hh          \
MemoryUtil.hh             \
Stopwatch.hh              \
Report.hh                 \
TimeBinFunction.hh        \
Util.hh                   \
NanoCubeInsert.hh         \
NanoCubeQuery.hh          \
NanoCubeQueryException.hh \
NanoCubeTimeQuery.hh      \
NanoCubeQueryResult.hh    \
NanoCubeReportBuilder.hh  \
NanoCubeSummary.hh        \
NanoCubeSchema.hh         \
NanoCube.hh               \
TimeSeriesEntryType.hh    \
DumpFile.hh               \
Tuple.hh                  \
Query.hh                  \
QueryParser.hh            \
QueryResult.hh            \
vector.hh                 \
Server.hh


SOURCES =                    \
Stopwatch.cc                 \
MemoryUtil.cc                \
QuadTreeNode.cc              \
TimeSeries.cc                \
Common.cc                    \
DumpFile.cc                  \
QuadTree.cc                  \
MercatorProjection.cc        \
NanoCubeQueryException.cc    \
TimeBinFunction.cc           \
Report.cc                    \
Util.cc                      \
NanoCubeSummary.cc           \
NanoCubeSchema.cc            \
NanoCubeQueryResult.cc       \
Query.cc                     \
QueryParser.cc               \
QueryResult.cc               \
Server.cc                    \
vector.cc                    \
mongoose.c                   \
nc.cc

#stree_serve.cc

#QuadTreeSerialization.cc     \


#test_quadtree_add.cc




# test_quadtree_range_query.cc



# dump_landlines.cc


# dump_landlines.cc




#


# test_load_and_build_tile.cc

#


#test_search_stree.cc


#

# serve.cc
# test_qt_serialization.cc
# test_stree_serialization.cc

# points2qtts3.cc


#points2qt.cc

#test_search_stree.cc

#
#serve.cc \
#mongoose.c


#STreeSerialization.hh \
#FlatTreeSerialization.hh \
#TimeSeriesSerialization.hh \
#QuadTreeSerialization.hh \

