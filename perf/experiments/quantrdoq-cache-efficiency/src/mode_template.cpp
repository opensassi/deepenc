#include "mode_template.h"

AccessContext* create_template_2x2()  { return new TemplateContext<2,2>(); }
AccessContext* create_template_3x3()  { return new TemplateContext<3,3>(); }
AccessContext* create_template_4x4()  { return new TemplateContext<4,4>(); }
AccessContext* create_template_5x5()  { return new TemplateContext<5,5>(); }
AccessContext* create_template_6x6()  { return new TemplateContext<6,6>(); }
