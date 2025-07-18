#include "checks.h"

void test_union()
{
	VERIFY_BOUNDS(0, 0, 0, 0, ssd1306_bounds_union(
			&(ssd1306_bounds_t){ },
			&(ssd1306_bounds_t){ }));

	VERIFY_BOUNDS(0, 0, 30, 50, ssd1306_bounds_union(
			&(ssd1306_bounds_t){ },
			&(ssd1306_bounds_t){ x0:  20, x1: 30 , y0:  40, y1: 50  }));
	VERIFY_BOUNDS(19, 39, 31, 51, ssd1306_bounds_union(
			&(ssd1306_bounds_t){ x0: 19 , x1:  31, y0: 39 , y1:  51 },
			&(ssd1306_bounds_t){ x0:  20, x1: 30 , y0:  40, y1: 50  }));
	VERIFY_BOUNDS(20, 40, 30, 50, ssd1306_bounds_union(
			&(ssd1306_bounds_t){ x0:  21, x1: 29 , y0:  41, y1: 49  },
			&(ssd1306_bounds_t){ x0: 20,   x1: 30, y0: 40 , y1:  50 }));
	VERIFY_BOUNDS(19, 39, 30, 50, ssd1306_bounds_union(
			&(ssd1306_bounds_t){ x0: 19 , x1: 29 , y0: 39 , y1: 49  },
			&(ssd1306_bounds_t){ x0:  20,  x1: 30, y0:  40, y1:  50 }));
	VERIFY_BOUNDS(20, 40, 31, 51, ssd1306_bounds_union(
			&(ssd1306_bounds_t){ x0:  21, x1:  31, y0:  41, y1:  51 },
			&(ssd1306_bounds_t){ x0: 20,  x1: 30 , y0: 40 , y1: 50  }));

	VERIFY_BOUNDS(0, 0, 30, 50, ssd1306_bounds_union(
			&(ssd1306_bounds_t){ x0:  20, x1: 30 , y0:  40, y1: 50  },
			&(ssd1306_bounds_t){ }));
	VERIFY_BOUNDS(19, 39, 31, 51, ssd1306_bounds_union(
			&(ssd1306_bounds_t){ x0:  20, x1: 30 , y0:  40, y1: 50  },
			&(ssd1306_bounds_t){ x0: 19 , x1:  31, y0: 39 , y1:  51 }));
	VERIFY_BOUNDS(20, 40, 30, 50, ssd1306_bounds_union(
			&(ssd1306_bounds_t){ x0: 20,   x1: 30, y0: 40 , y1:  50 },
			&(ssd1306_bounds_t){ x0:  21, x1: 29 , y0:  41, y1: 49  }));
	VERIFY_BOUNDS(19, 39, 30, 50, ssd1306_bounds_union(
			&(ssd1306_bounds_t){ x0:  20,  x1: 30, y0:  40, y1:  50 },
			&(ssd1306_bounds_t){ x0: 19 , x1: 29 , y0: 39 , y1: 49  }));
	VERIFY_BOUNDS(20, 40, 31, 51, ssd1306_bounds_union(
			&(ssd1306_bounds_t){ x0: 20,  x1: 30 , y0: 40 , y1: 50  },
			&(ssd1306_bounds_t){ x0:  21, x1:  31, y0:  41, y1:  51 }));
}

void test_intersect()
{
	ABORT_IF_NOT_NULL(ssd1306_bounds_intersect(
			&(ssd1306_bounds_t){ },
			&(ssd1306_bounds_t){ }));

	ABORT_IF_NOT_NULL(ssd1306_bounds_intersect(
			&(ssd1306_bounds_t){ },
			&(ssd1306_bounds_t){ x0:  20, x1: 30 , y0:  40, y1: 50  }));
	ABORT_IF_NOT_NULL(ssd1306_bounds_intersect(
			&(ssd1306_bounds_t){ x0: 10, x1: 20, y0: 10, y1: 20 },
			&(ssd1306_bounds_t){ x0: 20, x1: 30, y0: 20, y1: 30 }));
	VERIFY_BOUNDS(20, 40, 30, 50, ssd1306_bounds_intersect(
			&(ssd1306_bounds_t){ x0: 19 , x1:  31, y0: 39 , y1:  51 },
			&(ssd1306_bounds_t){ x0:  20, x1: 30 , y0:  40, y1: 50  }));
	VERIFY_BOUNDS(21, 41, 29, 49, ssd1306_bounds_intersect(
			&(ssd1306_bounds_t){ x0:  21, x1: 29 , y0:  41, y1: 49  },
			&(ssd1306_bounds_t){ x0: 20,   x1: 30, y0: 40 , y1:  50 }));
	VERIFY_BOUNDS(20, 40, 29, 49, ssd1306_bounds_intersect(
			&(ssd1306_bounds_t){ x0: 19 , x1: 29 , y0: 39 , y1: 49  },
			&(ssd1306_bounds_t){ x0:  20,  x1: 30, y0:  40, y1:  50 }));
	VERIFY_BOUNDS(21, 41, 30, 50, ssd1306_bounds_intersect(
			&(ssd1306_bounds_t){ x0:  21, x1:  31, y0:  41, y1:  51 },
			&(ssd1306_bounds_t){ x0: 20,  x1: 30 , y0: 40 , y1: 50  }));

	ABORT_IF_NOT_NULL(ssd1306_bounds_intersect(
			&(ssd1306_bounds_t){ x0:  20, x1: 30 , y0:  40, y1: 50  },
			&(ssd1306_bounds_t){ }));
	ABORT_IF_NOT_NULL(ssd1306_bounds_intersect(
			&(ssd1306_bounds_t){ x0: 20, x1: 30, y0: 20, y1: 30 },
			&(ssd1306_bounds_t){ x0: 10, x1: 20, y0: 10, y1: 20 }));
	VERIFY_BOUNDS(20, 40, 30, 50, ssd1306_bounds_intersect(
			&(ssd1306_bounds_t){ x0:  20, x1: 30 , y0:  40, y1: 50  },
			&(ssd1306_bounds_t){ x0: 19 , x1:  31, y0: 39 , y1:  51 }));
	VERIFY_BOUNDS(21, 41, 29, 49, ssd1306_bounds_intersect(
			&(ssd1306_bounds_t){ x0: 20,   x1: 30, y0: 40 , y1:  50 },
			&(ssd1306_bounds_t){ x0:  21, x1: 29 , y0:  41, y1: 49  }));
	VERIFY_BOUNDS(20, 40, 29, 49, ssd1306_bounds_intersect(
			&(ssd1306_bounds_t){ x0:  20,  x1: 30, y0:  40, y1:  50 },
			&(ssd1306_bounds_t){ x0: 19 , x1: 29 , y0: 39 , y1: 49  }));
	VERIFY_BOUNDS(21, 41, 30, 50, ssd1306_bounds_intersect(
			&(ssd1306_bounds_t){ x0: 20,  x1: 30 , y0: 40 , y1: 50  },
			&(ssd1306_bounds_t){ x0:  21, x1:  31, y0:  41, y1:  51 }));
}

void test_modify()
{
	VERIFY_BOUNDS(0, 0, 0, 0, ssd1306_bounds_resize(
			&(ssd1306_bounds_t){  },
			(ssd1306_size_t){  }));
	VERIFY_BOUNDS(0, 0, 1, 1, ssd1306_bounds_resize(
			&(ssd1306_bounds_t){  },
			(ssd1306_size_t){ w: 1, h: 1 }));
	VERIFY_BOUNDS(10, 20, 20, 30, ssd1306_bounds_move_to(
			&(ssd1306_bounds_t){ x0: 20, x1: 30, y0: 40, y1: 50 },
			(ssd1306_point_t){ x: 10, y: 20 }));
	VERIFY_BOUNDS(-10, -20, 0, -10, ssd1306_bounds_move_to(
			&(ssd1306_bounds_t){ x0: 20, x1: 30, y0: 40, y1: 50 },
			(ssd1306_point_t){ x: -10, y: -20 }));
	VERIFY_BOUNDS(30, 60, 40, 70, ssd1306_bounds_move_by(
			&(ssd1306_bounds_t){ x0: 20, x1: 30, y0: 40, y1: 50 },
			(ssd1306_point_t){ x: 10, y: 20 }));
	VERIFY_BOUNDS(10, 20, 20, 30, ssd1306_bounds_move_by(
			&(ssd1306_bounds_t){ x0: 20, x1: 30, y0: 40, y1: 50 },
			(ssd1306_point_t){ x: -10, y: -20 }));
}

void test_geom()
{
	LOG_I("testing geometry");

	test_union();
	test_intersect();
	test_modify();
}
