#ifndef __ruamoko_gui_Group_h
#define __ruamoko_gui_Group_h

#include "View.h"

@class Array;

@interface Group : View
{
	Array *views;
}
- (void) dealloc;
- (View*) addView: (View*)aView;
- (id) addViews: (Array*)viewlist;
- (void) moveTo: (int)x y:(int)y;
- (void) draw;
@end

#endif//__ruamoko_gui_Group_h
