name := "gcd-rocc"

organization := "edu.berkeley.cs"

version := "1.0-SNAPSHOT"

scalaVersion := "2.13.10"

resolvers ++= Seq(
  Resolver.sonatypeRepo("snapshots"),
  Resolver.sonatypeRepo("releases"),
  Resolver.mavenLocal
)

val chiselVersion = "3.6.0"

libraryDependencies ++= Seq(
  "edu.berkeley.cs" %% "chisel3" % chiselVersion,
  "edu.berkeley.cs" %% "rocketchip" % "1.6.0"
)

addCompilerPlugin("edu.berkeley.cs" % "chisel3-plugin" % chiselVersion cross CrossVersion.full)