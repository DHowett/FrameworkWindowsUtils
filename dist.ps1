Param(
	[string]$Version = "0.0.1",
	[string]$Cert = "cb9a86724771546a51851651d18e811a72f6e133"
)

$ErrorActionPreference='Stop'
rm -r -force O -EA:Ignore

### This must be run from the EWDK environment.
### It expects that you have SDK 10.0.22621.0 available.
msbuild .\FrameworkWindowsUtils.sln `
    /p:Platform=x64`;Configuration=Release `
    /p:OutDir=$PWD\O\ `
    /p:Reproducible=true `
    /p:TestCertificate=$Cert `
    /m
If ($LASTEXITCODE -Ne 0) { throw "Failed to Build" }

$BaseName="CrosEC-${Version}-$(git describe --always --abbrev)"
$Archive="${BaseName}.zip"
$DistDir="_dist\$BaseName"

rm -r -force _dist -EA:Ignore
mkdir $DistDir | out-null
mkdir "$DistDir\debug" | out-null
cp O\*.exe $DistDir
cp O\*.pdb "$DistDir\debug"
cp O\CrosEC\* $DistDir

signtool sign /sha1 $Cert /fd SHA256 /t http://timestamp.digicert.com (Get-Item $DistDir\*.exe | Select -Expand FullName)
If ($LASTEXITCODE -Ne 0) { throw "Failed to Sign" }

rm -force $Archive -EA:Ignore
tar -c --format zip --strip-components=1 -f $Archive _dist
If ($LASTEXITCODE -Ne 0) { throw "Failed to Archive" }
