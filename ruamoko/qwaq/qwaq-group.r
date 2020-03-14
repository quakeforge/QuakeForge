#include <Array.h>
#include "event.h"
#include "qwaq-group.h"

@implementation Array (Group)
- (void) makeObjectsPerformSelector: (SEL)selector
								 if: (condition_func)condition
							   with: (void *)data
{
	for (int i = 0; i < [self count]; i++) {
		if (condition (_objs[i], data)) {
			[_objs[i] performSelector: selector];
		}
	}
}

- (void) makeObjectsPerformSelector: (SEL)selector
						 withObject: (void *)anObject
								 if: (condition_func2)condition
							   with: (void *)data
{
	for (int i = 0; i < [self count]; i++) {
		if (condition (_objs[i], anObject, data)) {
			[_objs[i] performSelector: selector withObject: anObject];
		}
	}
}

- (void) makeReversedObjectsPerformSelector: (SEL)selector
{
	for (int i = [self count]; i-->0; ) {
		[_objs[i] performSelector: selector];
	}
}

- (void) makeReversedObjectsPerformSelector: (SEL)selector
								 withObject: (void *)anObject
{
	for (int i = [self count]; i-->0; ) {
		[_objs[i] performSelector: selector withObject: anObject];
	}
}

- (void) makeReversedObjectsPerformSelector: (SEL)selector
										 if: (condition_func)condition
									   with: (void *)data
{
	for (int i = [self count]; i-->0; ) {
		if (condition (_objs[i], data)) {
			[_objs[i] performSelector: selector];
		}
	}
}

- (void) makeReversedObjectsPerformSelector: (SEL)selector
								 withObject: (void *)anObject
										 if: (condition_func2)condition
									   with: (void *)data
{
	for (int i = [self count]; i-->0; ) {
		if (condition (_objs[i], anObject, data)) {
			[_objs[i] performSelector: selector withObject: anObject];
		}
	}
}
@end

@implementation Group

-initWithContext: (id) context
{
	if (!(self = [super init])) {
		return nil;
	}
	textContext = context;
	absRect = rect = { nil, [textContext size] };
	printf ("\n\nsize:%d %d\n\n", rect.extent.width, rect.extent.height);
	buffer = [DrawBuffer buffer: rect.extent];
	views = [[Array array] retain];
	return self;
}

-initWithRect: (Rect) rect
{
	if (!(self = [super initWithRect: rect])) {
		return nil;
	}
	views = [[Array array] retain];
	return self;
}

-(void)dealloc
{
	[views release];
}

-insert: (View *) view
{
	[views addObject: view];
	view.textContext = buffer;
	return self;
}

-remove: (View *) view
{
	int         index = [views indexOfObject: view];
	if (index != NotFound) {
		if (focused == index) {
			focused = -1;
		} else if (focused > index) {
			focused--;
		}
		[views removeObjectAtIndex: index];
	}
	return self;
}

static BOOL
not_dont_draw (id aView, void *aGroup)
{
	View       *view = aView;
	Group      *group = (Group *) aGroup;

	return !(view.options & ofDontDraw);
}

-draw
{
	[views makeObjectsPerformSelector: @selector(draw)
								   if: not_dont_draw
								 with: self];
	return self;
}

-handleEvent: (qwaq_event_t *) event
{
	[super handleEvent: event];
	if (event.what & qe_focused) {
		if (focused >= 0) {
			[[views objectAtIndex:focused] handleEvent: event];
		}
	} else if (event.what & qe_positional) {
	} else {
		// broadcast
		[views makeObjectsPerformSelector: @selector(draw) withObject: event];
	}
	return self;
}
@end
