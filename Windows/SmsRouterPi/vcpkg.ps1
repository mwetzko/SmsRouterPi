$ErrorActionPreference = "Stop"

Push-Location (Split-Path -Path $MyInvocation.MyCommand.Definition -Parent)

try {
	if (!(Test-Path ".\vcpkg\bootstrap-vcpkg.bat")) {
		& "git.exe" clone https://github.com/microsoft/vcpkg.git .\vcpkg
	}

	if (!(Test-Path ".\vcpkg\vcpkg.exe")) {
		& ".\vcpkg\bootstrap-vcpkg.bat"
	}

	@("x86", "x64") | ForEach-Object {
	
		$arch = $_
	
		@( , @("mailio:$($arch)-windows-static", "mailio\mailio.h") ) | ForEach-Object {
	
			$pack = $_[0]
			$header = $_[1]
	
			if (!(Test-Path (Join-Path -Path ".\vcpkg\installed\$($arch)-windows\include" -ChildPath $header))) {
				Write-Information "Installing $($pack)..."
				& ".\vcpkg\vcpkg.exe" install $pack
			}
	
		}
	
	}
}
finally {
	Pop-Location
}