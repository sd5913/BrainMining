import unreal
lib=unreal.MaterialEditingLibrary;assets=unreal.EditorAssetLibrary;folder='/Game/PopupClose/OfficeArt/Materials/'
for name,tint,grain in [('Wall',(.48,.47,.39,1),.012),('Ceiling',(.52,.52,.48,1),.018),('Carpet',(.11,.125,.118,1),.025),('Paper',(.73,.73,.69,1),0)]:
 m=unreal.load_asset(folder+'M_BR_'+name)
 for n in unreal.ObjectIterator():
  if n.get_outer()!=m:continue
  if isinstance(n,unreal.MaterialExpressionVectorParameter) and str(n.get_editor_property('parameter_name'))=='Tint':n.set_editor_property('default_value',unreal.LinearColor(*tint))
  if isinstance(n,unreal.MaterialExpressionNoise):n.set_editor_property('output_min',1-grain);n.set_editor_property('output_max',1+grain)
 lib.recompile_material(m);assets.save_loaded_asset(m,False)
for key in ['table2_Main_table','table2_Paint_mat']:
 m=unreal.load_asset(folder+'M_'+key);base=lib.get_material_property_input_node(m,unreal.MaterialProperty.MP_BASE_COLOR)
 if not isinstance(base,unreal.MaterialExpressionMultiply):
  des=lib.create_material_expression(m,unreal.MaterialExpressionDesaturation);fraction=lib.create_material_expression(m,unreal.MaterialExpressionConstant);fraction.set_editor_property('r',.5);lib.connect_material_expressions(fraction,'',des,'Fraction');assert lib.connect_material_expressions(base,'RGB',des,'')
  tint=lib.create_material_expression(m,unreal.MaterialExpressionVectorParameter);tint.set_editor_property('parameter_name','AgedWoodTint');tint.set_editor_property('default_value',unreal.LinearColor(.62,.65,.6,1))
  mult=lib.create_material_expression(m,unreal.MaterialExpressionMultiply);lib.connect_material_expressions(des,'',mult,'A');lib.connect_material_expressions(tint,'RGB',mult,'B');lib.connect_material_property(mult,'',unreal.MaterialProperty.MP_BASE_COLOR)
 nodes=[n for n in unreal.ObjectIterator(unreal.MaterialExpression) if n.get_outer()==m]
 source=next((n for n in nodes if isinstance(n,unreal.MaterialExpressionTextureSampleParameter2D) and str(n.get_editor_property('parameter_name'))=='BaseColor'),None)
 if not source:source=next(n for n in nodes if isinstance(n,unreal.MaterialExpressionVectorParameter) and str(n.get_editor_property('parameter_name'))=='Tint')
 for n in nodes:
  if isinstance(n,unreal.MaterialExpressionDesaturation):assert lib.connect_material_expressions(source,'RGB',n,'')
 lib.recompile_material(m);assets.save_loaded_asset(m,False)
print('REFINED_OFFICE_FINISHES')
screen=unreal.load_asset(folder+'M_table3_Monitor_screen_mat')
for n in unreal.ObjectIterator(unreal.MaterialExpressionVectorParameter):
 if n.get_outer()==screen:n.set_editor_property('default_value',unreal.LinearColor(.007,.015,.017,1))
lib.recompile_material(screen);assets.save_loaded_asset(screen,False)
