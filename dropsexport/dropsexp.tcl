puts "loading drops-export-module"

if { [catch { load libdropsexp[info sharedlibextension] dropsexp } result] } {
    puts "cannot load dropsexp" 
    puts "error: $result"
} {

    .ngmenu add cascade -label "DROPS-Export" -menu .ngmenu.dropsexp -underline 0
 
    menu .ngmenu.dropsexp
    .ngmenu.dropsexp add command -label "Export as DROPS-Mesh" \
    -command {
	set file [tk_getSaveFile  -filetypes "{ \"(Ext.) Gambit\" {*.msh}}" ]
	if {$file != ""} {
	    DE_ExportMesh $file $exportfiletype 
	}
    }

}
