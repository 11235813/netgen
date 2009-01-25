if { [catch { load libdemoapp.so demoapp }] } {
    puts "cannot load demoapp" 
}

.ngmenu add cascade -label "DemoApp" -menu .ngmenu.demo -underline 0
 
menu .ngmenu.demo
.ngmenu.demo add command -label "Choose me" \
    -command { DA_ChooseMe }

.ngmenu.demo add command -label "Print Mesh" \
    -command { DA_PrintMesh }

.ngmenu.demo add command -label "Compute Solution" \
    -command { DA_SetSolution }



puts "loading demo-application"

