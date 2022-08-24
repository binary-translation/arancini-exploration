#include <arancini/ir/node.h>
#include <arancini/ir/port.h>

using namespace arancini::ir;

bool port::accept(visitor &v)
{
	return v.visit_port(*this);
}
