import c4d

cube = c4d.BaseObject(c4d.Ocube)
if cube is None:
    raise RuntimeError("Failed to create a cube.")

doc.InsertObject(cube, None, None)
c4d.EventAdd()