// Configuration classes for integrating GCD RoCC into Chipyard
// Add these to generators/chipyard/src/main/scala/config/RoCCAcceleratorConfigs.scala

package chipyard

import org.chipsalliance.cde.config.{Config, Parameters}
import freechips.rocketchip.tile.{BuildRoCC, OpcodeSet}
import freechips.rocketchip.diplomacy.{LazyModule}

// GCD RoCC Configuration Fragment
class WithGCDRoCC extends Config((site, here, up) => {
  case BuildRoCC => up(BuildRoCC) ++ Seq(
    (p: Parameters) => {
      val gcd = LazyModule(new gcdrocc.GCDRoCC(OpcodeSet.custom0)(p))
      gcd
    })
})

// Complete configuration for testing
class GCDRoCCRocketConfig extends Config(
  new WithGCDRoCC ++                                              // use GCD RoCC accelerator
  new freechips.rocketchip.subsystem.WithNBigCores(1) ++
  new chipyard.config.AbstractConfig)