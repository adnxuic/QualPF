#pragma once
namespace qualpf::gui {
struct PeakSearchPresentationOptions {
    double minRange, maxRange;
    double minSearch, maxSearch;
    double threshold;
    int sensitivity;
    int numPeaks;
    int numPeaksTot;
    bool append;
};
struct SmoothingPresentationOptions { int method; int npoints_sg; int npoints_ave; int pol_order; };
struct BackgroundPresentationOptions { int btype; bool autob; int ncoef; int niterf; int nwinf; double minf; double maxf; };
}
