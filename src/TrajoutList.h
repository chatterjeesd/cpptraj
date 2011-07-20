#ifndef INC_TRAJOUTLIST_H
#define INC_TRAJOUTLIST_H
/// Class: TrajoutList
#include "CoordFileList.h"
#include "Frame.h"
class TrajoutList : public CoordFileList {
  public:
    TrajoutList();
    ~TrajoutList();
    // Add a traj file to the list with given access and associate with a parm
    int Add(char*,ArgList *A, AmberParm *);
    // Call write for all trajectories
    int Write(int, AmberParm*, Frame*);
    // Call end for all trajectories
    void Close();
};
#endif

