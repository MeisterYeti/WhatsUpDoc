#define DOSOMETHING(a,b) int a=b

int callMe(int i) {
	return i+1;
}

namespace A {
void init();
}
/*! head 1 */
void A::init() {
/*! test 1 */
DOSOMETHING(x,1);
/*! call 1 */
callMe(x);
}

namespace B {
/*! head 2 */
void init() {
/*! test 2 */
DOSOMETHING(y,2+3);
//! call 2
callMe(y);
}
}
