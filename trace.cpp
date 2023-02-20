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
		uint8_t SFRs[128]; // Special Function Registers 128 bytes
		uint8_t lower[128]; // Lower 128 bytes
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

extern "C" void set_trace_pc(uint16_t pc, bool post) {
	if (!post) {
		build_frame.pre.pc = pc;
	} else {
		build_frame.post.pc = pc;
	}
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

	//	if (!diff || in->pc != diff->pc) {
	//		push_reg(out, "pc", in->pc, 16, r, w);
	//	}

#define PUSH_SFR(name, reg_idx, bits) \
	//	do { \
//		if (!diff || in->SFRs[reg_idx] != diff->SFRs[reg_idx]) { \
//			push_reg(out, name, in->SFRs[reg_idx], bits, r, w); \
//		} \
//	} while (0) \
	// \
	//	// 7 Special Function Registers (SFRs). \
	//	PUSH_SFR("sbuf", REG_SBUF, 8); \
	//	PUSH_SFR("tcon", REG_TCON, 8); \
	//	PUSH_SFR("tmod", REG_TMOD, 8); \
	//	PUSH_SFR("scon", REG_SCON, 8);
	//	PUSH_SFR("pcon", REG_PCON, 8);
	//	PUSH_SFR("ip", REG_IP, 8);
	//	PUSH_SFR("ie", REG_IE, 8);
	//
	//	PUSH_SFR("psw", REG_PSW, 8);
	//
	//	PUSH_SFR("dpl", REG_DPL, 8);
	//	PUSH_SFR("dph", REG_DPH, 8);
	//	if (!diff || in->SFRs[REG_DPL] != diff->SFRs[REG_DPL] || in->SFRs[REG_DPH] != diff->SFRs[REG_DPH]) {
	//		uint16_t dptr = ((uint16_t)(in->SFRs[REG_DPH]) << 8) | in->SFRs[REG_DPL];
	//		push_reg(out, "dptr", dptr, 16, r, w);
	//	}
	//
	//	PUSH_SFR("sp", REG_SP, 8);
	//	PUSH_SFR("acc", REG_ACC, 8);
	//	PUSH_SFR("b", REG_B, 8);
	//
	//	PUSH_SFR("p0", REG_P0, 8);
	//	PUSH_SFR("p1", REG_P1, 8);
	//	PUSH_SFR("p2", REG_P2, 8);
	//	PUSH_SFR("p3", REG_P3, 8);
	//	PUSH_SFR("tl0", REG_TL0, 8);
	//	PUSH_SFR("tl1", REG_TL1, 8);
	//	PUSH_SFR("th0", REG_TH0, 8);
	//	PUSH_SFR("th1", REG_TH1, 8);
	//
	// #define PUSH_GPR(name, reg_idx, bits) \
//	do { \
//		if (!diff || in->lower[reg_idx] != diff->lower[reg_idx]) { \
//			push_reg(out, name, in->lower[reg_idx], bits, r, w); \
//		} \
//	} while (0)
	//	// 8 General Purpose Registers (lower).
	//	uint8_t bank = (in->SFRs[REG_PSW] & 0x18) >> 3;
	//	PUSH_GPR("r0", 0x8 * bank, 8);
	//	PUSH_GPR("r1", 0x8 * bank + 1, 8);
	//	PUSH_GPR("r2", 0x8 * bank + 2, 8);
	//	PUSH_GPR("r3", 0x8 * bank + 3, 8);
	//	PUSH_GPR("r4", 0x8 * bank + 4, 8);
	//	PUSH_GPR("r5", 0x8 * bank + 5, 8);
	//	PUSH_GPR("r6", 0x8 * bank + 6, 8);
	//	PUSH_GPR("r7", 0x8 * bank + 7, 8);
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
	to->registers.emplace(std::string{ name }, TraceReg{ v, bits });
}

void mem_push(uint16_t addr, uint8_t v, bool w) {
	TraceOperands8051 *to = w ? &build_frame.post : &build_frame.pre;
	to->mems.push_back(TraceMem{ addr, v });
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
