#include "trace.h"
#include "trace.container.hpp"
#include "emu8051.h"
#include <map>
#include <string>

#define LOG_PREFIX "\033[36m[TRACE]\033[0m "

#define eprintf(...) fprintf(stderr, LOG_PREFIX __VA_ARGS__)

typedef struct trace_mem_t {
		uint16_t addr;
		uint8_t val;
} TraceMem;

typedef struct trace_reg_t {
		uint16_t value;
		size_t bits;
} TraceReg;

typedef struct trace_operands_t {
//		uint8_t SFRs[128]; // Special Function Registers 128 bytes
//		uint8_t lower[128]; // Lower 128 bytes
		uint16_t pc; // Program Counter; outside memory area
		std::map<std::string, TraceReg> registers;
		std::vector<TraceMem> mems;
} TraceOperands8051;

typedef struct trace_frame_t {
		uint8_t op[3];
		size_t op_size;
		TraceOperands8051 pre;
		TraceOperands8051 post;
} TraceFrame8051;

TraceFrame8051 build_frame;

static void close_writer(SerializedTrace::TraceContainerWriter *w) {
	eprintf("closing...\n");
	w->finish();
	delete w;
	eprintf("closed.\n");
}

std::unique_ptr<SerializedTrace::TraceContainerWriter, decltype(&close_writer)> writer(nullptr, close_writer);

extern "C" int trace_open(const char *filename) {
	eprintf("opening...\n");
	if (writer) {
		eprintf("already open.\n");
		return 0;
	}

	tracer *trac = new tracer();
	trac->set_name("emu8051");
	trac->set_version("");

	target *tgt = new target();
	tgt->set_path("");
	tgt->set_md5sum("");

	fstats *fst = new fstats();
	fst->set_size(0);
	fst->set_atime(0.0);
	fst->set_mtime(0.0);
	fst->set_ctime(0.0);

	meta_frame meta;
	meta.set_allocated_tracer(trac);
	meta.set_allocated_target(tgt);
	meta.set_allocated_fstats(fst);
	meta.set_user("");
	meta.set_host("");
	meta.set_time(0.0);

	try {
		writer.reset(new SerializedTrace::TraceContainerWriter(filename, meta, frame_arch_8051, 0));
	} catch (std::exception &e) {
		eprintf("open failed: %s\n", e.what());
		return 0;
	}
	eprintf("opened\n");
	return 1;
}

extern "C" void trace_close(void) {
	writer.reset(nullptr);
}

extern "C" int trace_is_open(void) {
	return !!writer;
}

extern "C" void set_trace_op(const char *op, uint size) {
	memcpy(build_frame.op, op, size);
	build_frame.op_size = size;
}

void push_reg(operand_value_list *out, const char *name, uint16_t v, size_t bits, bool r, bool w) {
	reg_operand *ro = new reg_operand();
	ro->set_name(name);
	operand_info_specific *s = new operand_info_specific();
	s->set_allocated_reg_operand(ro);

	operand_usage *u = new operand_usage();
	u->set_read(r);
	u->set_written(w);
	u->set_index(false);
	u->set_base(false);

	taint_info *ti = new taint_info();

	operand_info *i = out->add_elem();
	i->set_allocated_operand_info_specific(s);
	i->set_bit_length((int32_t)bits);
	i->set_allocated_operand_usage(u);
	i->set_allocated_taint_info(ti);
	uint8_t va[2] = { (uint8_t)v, (uint8_t)(v >> 8) };
	i->set_value(std::string((const char *)va, bits / 8));
}

static void push_regs(operand_value_list *out, TraceOperands8051 *in, bool r, bool w, TraceOperands8051 *diff) {
	for (const auto &[k, reg] : in->registers) {
		push_reg(out, k.c_str(), reg.value, reg.bits, r, w);
	}
}

static void push_mems(operand_value_list *out, TraceOperands8051 *in, bool r, bool w) {
	for (const auto &m : in->mems) {
		mem_operand *mo = new mem_operand();
		mo->set_address(m.addr);
		operand_info_specific *s = new operand_info_specific();
		s->set_allocated_mem_operand(mo);

		operand_usage *u = new operand_usage();
		u->set_read(r);
		u->set_written(w);
		u->set_index(false);
		u->set_base(false);

		taint_info *ti = new taint_info();

		operand_info *i = out->add_elem();
		i->set_allocated_operand_info_specific(s);
		i->set_bit_length(8);
		i->set_allocated_operand_usage(u);
		i->set_allocated_taint_info(ti);
		i->set_value(std::string((const char *)&m.val, 1));
	}
}

void register_push(const char *name, uint16_t v, size_t bits, bool w) {
	TraceOperands8051 *to = w ? &build_frame.post : &build_frame.pre;
	if (w) {
		to->registers.insert_or_assign(std::string{ name }, TraceReg{ v, bits });
	} else {
		to->registers.emplace(std::string{ name }, TraceReg{ v, bits });
	}
}

void mem_push(uint16_t addr, uint8_t v, bool w) {
	TraceOperands8051 *to = w ? &build_frame.post : &build_frame.pre;
	to->mems.push_back(TraceMem{ addr, v });
}

void pc_push(uint16_t v, bool pre) {
	TraceOperands8051 *to = pre ? &build_frame.pre : &build_frame.post;
	to->pc = v;
}

extern "C" void trace_push() {
	if (!writer) {
		eprintf("tried to push but not opened.\n");
	}

	auto tf = &build_frame;
	operand_value_list *pre = new operand_value_list();
	push_regs(pre, &tf->pre, true, false, nullptr);
	push_mems(pre, &tf->pre, true, false);

	operand_value_list *post = new operand_value_list();
	push_regs(post, &tf->post, false, true, &tf->pre);
	push_mems(post, &tf->post, false, true);

	std_frame *sf = new std_frame();
	sf->set_address(tf->pre.pc);
	sf->set_thread_id(0);
	sf->set_rawbytes(std::string((const char *)tf->op, tf->op_size));
	sf->set_allocated_operand_pre_list(pre);
	sf->set_allocated_operand_post_list(post);

	frame f;
	f.set_allocated_std_frame(sf);

	writer->add(f);

	tf->pre.registers.clear();
	tf->pre.mems.clear();
	tf->post.registers.clear();
	tf->post.mems.clear();
}
