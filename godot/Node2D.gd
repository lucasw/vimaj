extends Node2D


# Declare member variables here. Examples:
# var a = 2
# var b = "text"
var image_list = []
var index = 0
var cur_path

# Called when the node enters the scene tree for the first time.
func _ready():
	$FileDialog.hide()
	$Button.connect("pressed", self, "open_dir", ["open_dir_test"])
	$FileDialog.mode = FileDialog.MODE_OPEN_DIR
	# $FileDialog.connect("file_selected", self, "image_file_selected")
	$FileDialog.connect("dir_selected", self, "image_dir_selected")

func open_dir(button_name):
	print(button_name)
	$FileDialog.show()
# Called every frame. 'delta' is the elapsed time since the previous frame.
#func _process(delta):
#	pass

func image_dir_selected(path):

	print(cur_path)
	var dir = Directory.new()

	if dir.open(path) != OK:
		print("An error occurred when trying to access the path.")
		return
		
	dir.list_dir_begin()
	var image_names = []
	while true:
		var file_name = dir.get_next()
		if file_name == "":
			break
		if dir.current_is_dir():
			continue
		var num = len(file_name)
		if num < 4:
			continue
		var postfix = file_name.right(num - 3)
		if postfix != "jpg" and postfix != "png":
			continue
		image_names = image_names + [file_name]
		
	for image_name in image_names:
		print(image_name)
	cur_path = path
	image_list = image_names
	index = 0
	if len(image_list) == 0:
		return
	image_file_selected(path + "/" + image_names[0])

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
	
func change_image(new_index):
	var num = len(image_list)
	if num == 0:
		return
	# index = new_index % num
	if new_index >= num:
		return
	if new_index < 0:
		return
	index = new_index
	image_file_selected(cur_path + "/" + image_list[index])
	
func _input(event):
	if event.is_action_pressed("ui_left"):
		change_image(index - 1)
		
	if event.is_action_pressed("ui_right"):
		change_image(index + 1)
