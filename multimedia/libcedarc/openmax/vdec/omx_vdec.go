package omx_vdec

import (
    "android/soong/android"
    "android/soong/cc"
    "fmt"
)

func init() {
    android.RegisterModuleType("omx_vdec_defaults", omxVdecDefaultsFactory)
}

func omxVdecDefaultsFactory() android.Module {
    module := cc.DefaultsFactory()
    android.AddLoadHook(module, omxVdecDefaults)

    return module
}

func omxVdecDefaults(ctx android.LoadHookContext) {
    type props struct {
        Include_dirs []string
    }

    p := &props{}

    board := ctx.AConfig().VendorConfig("vendor").String("board")
    switch board{
        case "ceres":
            p.Include_dirs = append(p.Include_dirs,"frameworks/av/media/libcedarc/openmax/omxcore/tablelist/ceres")
        case "venus":
            p.Include_dirs = append(p.Include_dirs,"frameworks/av/media/libcedarc/openmax/omxcore/tablelist/venus")
        case "petrel":
            p.Include_dirs = append(p.Include_dirs,"frameworks/av/media/libcedarc/openmax/omxcore/tablelist/petrel")
        case "dolphin":
            p.Include_dirs = append(p.Include_dirs,"frameworks/av/media/libcedarc/openmax/omxcore/tablelist/dolphin")
        case "tulip":
            p.Include_dirs = append(p.Include_dirs,"frameworks/av/media/libcedarc/openmax/omxcore/tablelist/tulip")
        case "cupid":
            p.Include_dirs = append(p.Include_dirs,"frameworks/av/media/libcedarc/openmax/omxcore/tablelist/cupid")
        case "mercury":
            p.Include_dirs = append(p.Include_dirs,"frameworks/av/media/libcedarc/openmax/omxcore/tablelist/mercury")
        default:
            p.Include_dirs = append(p.Include_dirs,"frameworks/av/media/libcedarc/openmax/omxcore/tablelist/default")
            fmt.Println("Include_dirs has NOT been set. Please check it.")
    }

    ctx.AppendProperties(p)
}
