extends Node2D


# Declare member variables here. Examples:
# var a = 2
# var b = "text"


# Called when the node enters the scene tree for the first time.
func _ready():
	$FileDialog.hide()
	$Button.connect("pressed", self, "open_dir", ["open_dir_test"])
	$FileDialog.connect("file_selected", self, "image_file_selected")

func open_dir(button_name):
	print(button_name)
	$FileDialog.show()
# Called every frame. 'delta' is the elapsed time since the previous frame.
#func _process(delta):
#	pass

func image_file_selected(path):
	print(path)

	var image = Image.new()
	var err = image.load(path) # resource is loaded when line is executed
	# 0 is OK

	if err != OK:
		print(err)
		return
	var image_texture = ImageTexture.new()
	image_texture.create_from_image(image)

	# this works only for project resources
#	var stream_texture = load(path)
#	if stream_texture == null:
#		print("null streamtexture")
#		return
#	var image_texture = ImageTexture.new()
#	var image = stream_texture.get_data()
#	
#	# image.lock() # so i can modify pixel data
#	image_texture.create_from_image(image, 0)
	
	$Sprite.texture = image_texture
	
	var img_size = image.get_size()
	print(img_size)
	# scale the image to the size of the window
	var screen_size = get_viewport().get_visible_rect().size
	print(screen_size)
	
	var sc_x = screen_size.x / img_size.x
	var sc_y = screen_size.y / img_size.y
	
	if sc_x < sc_y:
		sc_y = sc_x
	else:
		sc_x = sc_y
	
	$Sprite.set_scale(Vector2(sc_x, sc_y))
	
#
