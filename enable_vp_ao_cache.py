# Replace the code in aftersceneload.py with the code from this file 
# When it is run by CallPython, doc is already defined as the newly loaded document
# If Ambient Occlusion is found in the render settings for the loaded scene then it will enable the Caching checkbox

import c4d

def main() -> None:
    # Get the render settings
    rd = doc.GetActiveRenderData()

    vp = rd.GetFirstVideoPost()

    found = False

    while vp is not None:
        if vp.CheckType(c4d.VPambientocclusion):
            found = True
            break
        vp = vp.GetNext()

    if found:
        print("Found Ambient Occlusion")
        # Enable Ambient Occlusion Caching
        vp[c4d.VPAMBIENTOCCLUSION_CACHE_ENABLE] = True
    else:
        print("Did not find Ambient Occlusion")

    doc.SetActiveRenderData(rd)

    # Update the render settings
    c4d.EventAdd()

if __name__ == '__main__':
    main()