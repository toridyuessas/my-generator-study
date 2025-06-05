package gcdrocc

import chisel3._
import chisel3.util._
import org.chipsalliance.cde.config._
import freechips.rocketchip.subsystem._
import freechips.rocketchip.diplomacy._
import freechips.rocketchip.rocket._
import freechips.rocketchip.tilelink._
import freechips.rocketchip.tile._

// Custom instruction formats for GCD RoCC
// funct=0: start GCD computation with two values
// funct=1: read GCD result
object GCDInstr {
  val START = 0.U
  val READ  = 1.U
}

class GCDRoCC(opcodes: OpcodeSet)(implicit p: Parameters) extends LazyRoCC(opcodes) {
  override lazy val module = new GCDRoCCModuleImp(this)
}

class GCDRoCCModuleImp(outer: GCDRoCC)(implicit p: Parameters) extends LazyRoCCModuleImp(outer) {
  
  // GCD computation states
  val s_idle :: s_compute :: s_done :: Nil = Enum(3)
  val state = RegInit(s_idle)
  
  // Registers for GCD computation
  val a = Reg(UInt(64.W))
  val b = Reg(UInt(64.W))
  val result = Reg(UInt(64.W))
  
  // Command queue to handle incoming RoCC commands
  val cmd = Queue(io.cmd)
  
  // Default outputs
  io.resp.valid := false.B
  io.resp.bits := DontCare
  io.busy := state =/= s_idle
  io.interrupt := false.B
  cmd.ready := false.B
  
  // Main state machine
  switch(state) {
    is(s_idle) {
      when(cmd.valid) {
        switch(cmd.bits.inst.funct) {
          is(GCDInstr.START) {
            // Start GCD computation with rs1 and rs2
            a := cmd.bits.rs1
            b := cmd.bits.rs2
            state := s_compute
            cmd.ready := true.B
            printf("GCD RoCC: Starting computation with a=%d, b=%d\n", cmd.bits.rs1, cmd.bits.rs2)
          }
          is(GCDInstr.READ) {
            // Return the computed result
            io.resp.valid := true.B
            io.resp.bits.rd := cmd.bits.inst.rd
            io.resp.bits.data := result
            cmd.ready := true.B
            printf("GCD RoCC: Reading result=%d\n", result)
          }
        }
      }
    }
    
    is(s_compute) {
      // Euclidean GCD algorithm
      when(b === 0.U) {
        result := a
        state := s_done
        printf("GCD RoCC: Computation complete, result=%d\n", a)
      } .elsewhen(a > b) {
        a := a - b
      } .otherwise {
        b := b - a
      }
    }
    
    is(s_done) {
      // Wait in done state until next command
      state := s_idle
    }
  }
  
  // Memory interface - explicit initialization
  io.mem.req.valid := false.B
  io.mem.req.bits.addr := 0.U
  io.mem.req.bits.tag := 0.U
  io.mem.req.bits.cmd := 0.U
  io.mem.req.bits.size := 0.U
  io.mem.req.bits.signed := false.B
  io.mem.req.bits.dprv := 0.U
  io.mem.req.bits.dv := false.B
  io.mem.req.bits.phys := false.B
  io.mem.req.bits.no_alloc := false.B
  io.mem.req.bits.no_xcpt := false.B
  io.mem.req.bits.data := 0.U
  io.mem.req.bits.mask := 0.U
  
  io.mem.s1_kill := false.B
  io.mem.s1_data.data := 0.U
  io.mem.s1_data.mask := 0.U
  io.mem.s2_kill := false.B
  io.mem.keep_clock_enabled := false.B
  
  // PTW interface - explicit initialization  
  io.ptw.foreach { ptw =>
    ptw.req.valid := false.B
    ptw.req.bits := DontCare  // PTW is complex, DontCare is safer here
  }
  
  // FPU interface - explicit initialization
  io.fpu_req.valid := false.B
  io.fpu_req.bits := DontCare  // FPU is complex, DontCare is safer here
}