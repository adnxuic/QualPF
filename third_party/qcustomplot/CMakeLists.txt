add_library(qualpf_qcustomplot OBJECT
    qcustomplot.cpp
    qcustomplot.h
)
qualpf_configure_qt_boundary(qualpf_qcustomplot Widgets PrintSupport)
