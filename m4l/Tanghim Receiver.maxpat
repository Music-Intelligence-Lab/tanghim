{
	"patcher": {
		"fileversion": 1,
		"appversion": {
			"major": 8,
			"minor": 5,
			"revision": 5,
			"architecture": "x64",
			"modernui": 1
		},
		"classnamespace": "box",
		"rect": [
			100.0,
			100.0,
			900.0,
			600.0
		],
		"bglocked": 0,
		"openinpresentation": 1,
		"default_fontsize": 12.0,
		"default_fontface": 0,
		"default_fontname": "Arial",
		"gridonopen": 1,
		"gridsize": [
			15.0,
			15.0
		],
		"gridsnaponopen": 1,
		"objectsnaponopen": 1,
		"statusbarvisible": 2,
		"toolbarvisible": 1,
		"lefttoolbarpinned": 0,
		"toptoolbarpinned": 0,
		"righttoolbarpinned": 0,
		"bottomtoolbarpinned": 0,
		"toolbars_unpinned_last_save": 0,
		"tallnewobj": 0,
		"boxanimatetime": 200,
		"enablehscroll": 1,
		"enablevscroll": 1,
		"devicewidth": 400.0,
		"description": "MTS-ESP microtuning via MPE or Pitch Bend",
		"digest": "",
		"tags": "",
		"style": "",
		"subpatcher_template": "",
		"assistshowspatchername": 0,
		"boxes": [
			{
				"box": {
					"id": "obj-1",
					"maxclass": "comment",
					"numinlets": 1,
					"numoutlets": 0,
					"patching_rect": [
						30,
						460,
						200,
						22
					],
					"presentation": 1,
					"presentation_rect": [
						10,
						8,
						200,
						22
					],
					"fontsize": 14.0,
					"text": "Tanghim",
					"textcolor": [
						1,
						1,
						1,
						1
					]
				}
			},
			{
				"box": {
					"id": "obj-2",
					"maxclass": "comment",
					"numinlets": 1,
					"numoutlets": 0,
					"patching_rect": [
						30,
						490,
						60,
						18
					],
					"presentation": 1,
					"presentation_rect": [
						10,
						80,
						60,
						18
					],
					"fontsize": 10.0,
					"text": "Mode:",
					"textcolor": [
						0.7,
						0.7,
						0.7,
						1
					]
				}
			},
			{
				"box": {
					"id": "obj-3",
					"maxclass": "comment",
					"numinlets": 1,
					"numoutlets": 0,
					"patching_rect": [
						200,
						460,
						200,
						18
					],
					"presentation": 1,
					"presentation_rect": [
						10,
						100,
						200,
						18
					],
					"fontsize": 10.0,
					"text": "",
					"textcolor": [
						0.5,
						0.5,
						0.5,
						1
					]
				}
			},
			{
				"box": {
					"id": "obj-4",
					"maxclass": "newobj",
					"numinlets": 1,
					"numoutlets": 1,
					"patching_rect": [
						30,
						45,
						48,
						22
					],
					"text": "midiin",
					"outlettype": [
						"int"
					]
				}
			},
			{
				"box": {
					"id": "obj-5",
					"maxclass": "newobj",
					"numinlets": 1,
					"numoutlets": 8,
					"patching_rect": [
						30,
						80,
						300,
						22
					],
					"text": "midiparse",
					"outlettype": [
						"",
						"",
						"",
						"int",
						"int",
						"",
						"int",
						""
					]
				}
			},
			{
				"box": {
					"id": "obj-6",
					"maxclass": "newobj",
					"numinlets": 2,
					"numoutlets": 1,
					"patching_rect": [
						30,
						150,
						150,
						22
					],
					"text": "js mts_midi_effect.js",
					"outlettype": [
						""
					]
				}
			},
			{
				"box": {
					"id": "obj-7",
					"maxclass": "newobj",
					"numinlets": 7,
					"numoutlets": 1,
					"patching_rect": [
						200,
						185,
						200,
						22
					],
					"text": "midiformat",
					"outlettype": [
						"int"
					]
				}
			},
			{
				"box": {
					"id": "obj-8",
					"maxclass": "newobj",
					"numinlets": 1,
					"numoutlets": 0,
					"patching_rect": [
						30,
						220,
						52,
						22
					],
					"text": "midiout"
				}
			},
			{
				"box": {
					"id": "obj-9",
					"maxclass": "newobj",
					"numinlets": 1,
					"numoutlets": 1,
					"patching_rect": [
						450,
						45,
						58,
						22
					],
					"text": "loadbang",
					"outlettype": [
						"bang"
					]
				}
			},
			{
				"box": {
					"id": "obj-10",
					"maxclass": "newobj",
					"numinlets": 1,
					"numoutlets": 2,
					"patching_rect": [
						450,
						80,
						40,
						22
					],
					"text": "t b b",
					"outlettype": [
						"bang",
						"bang"
					]
				}
			},
			{
				"box": {
					"id": "obj-11",
					"maxclass": "newobj",
					"numinlets": 2,
					"numoutlets": 1,
					"patching_rect": [
						450,
						115,
						72,
						22
					],
					"text": "delay 2000",
					"outlettype": [
						"bang"
					]
				}
			},
			{
				"box": {
					"id": "obj-12",
					"maxclass": "message",
					"numinlets": 2,
					"numoutlets": 1,
					"patching_rect": [
						450,
						150,
						240,
						22
					],
					"outlettype": [
						""
					],
					"text": "plug_vst3 \"Tanghim Receiver\""
				}
			},
			{
				"box": {
					"id": "obj-13",
					"maxclass": "newobj",
					"numinlets": 2,
					"numoutlets": 1,
					"patching_rect": [
						560,
						115,
						72,
						22
					],
					"text": "delay 3000",
					"outlettype": [
						"bang"
					]
				}
			},
			{
				"box": {
					"id": "obj-14",
					"maxclass": "newobj",
					"numinlets": 2,
					"numoutlets": 1,
					"patching_rect": [
						560,
						150,
						72,
						22
					],
					"text": "metro 100",
					"outlettype": [
						"bang"
					]
				}
			},
			{
				"box": {
					"id": "obj-15",
					"maxclass": "newobj",
					"numinlets": 1,
					"numoutlets": 3,
					"patching_rect": [
						560,
						185,
						72,
						22
					],
					"text": "uzi 128 0",
					"outlettype": [
						"bang",
						"int",
						"int"
					]
				}
			},
			{
				"box": {
					"id": "obj-16",
					"maxclass": "newobj",
					"numinlets": 2,
					"numoutlets": 1,
					"patching_rect": [
						610,
						220,
						36,
						22
					],
					"text": "+ 4",
					"outlettype": [
						"int"
					]
				}
			},
			{
				"box": {
					"id": "obj-17",
					"maxclass": "newobj",
					"numinlets": 1,
					"numoutlets": 1,
					"patching_rect": [
						610,
						255,
						78,
						22
					],
					"text": "prepend get",
					"outlettype": [
						""
					]
				}
			},
			{
				"box": {
					"id": "obj-18",
					"maxclass": "newobj",
					"numinlets": 1,
					"numoutlets": 1,
					"patching_rect": [
						450,
						255,
						50,
						22
					],
					"text": "sig~ 0.",
					"outlettype": [
						"signal"
					]
				}
			},
			{
				"box": {
					"id": "obj-19",
					"maxclass": "newobj",
					"numinlets": 2,
					"numoutlets": 8,
					"patching_rect": [
						450,
						295,
						200,
						22
					],
					"text": "vst~",
					"outlettype": [
						"signal",
						"signal",
						"",
						"list",
						"int",
						"",
						"",
						""
					]
				}
			},
			{
				"box": {
					"id": "obj-20",
					"maxclass": "message",
					"numinlets": 2,
					"numoutlets": 1,
					"patching_rect": [
						700,
						255,
						38,
						22
					],
					"outlettype": [
						""
					],
					"text": "open"
				}
			},
			{
				"box": {
					"id": "obj-21",
					"maxclass": "newobj",
					"numinlets": 2,
					"numoutlets": 2,
					"patching_rect": [
						530,
						340,
						72,
						22
					],
					"text": "unpack 0 0.",
					"outlettype": [
						"int",
						"float"
					]
				}
			},
			{
				"box": {
					"id": "obj-22",
					"maxclass": "newobj",
					"numinlets": 2,
					"numoutlets": 1,
					"patching_rect": [
						530,
						375,
						36,
						22
					],
					"text": "- 4",
					"outlettype": [
						"int"
					]
				}
			},
			{
				"box": {
					"id": "obj-23",
					"maxclass": "newobj",
					"numinlets": 1,
					"numoutlets": 1,
					"patching_rect": [
						580,
						375,
						152,
						22
					],
					"text": "expr $f1 * 9600. - 4800.",
					"outlettype": [
						""
					]
				}
			},
			{
				"box": {
					"id": "obj-24",
					"maxclass": "newobj",
					"numinlets": 2,
					"numoutlets": 1,
					"patching_rect": [
						530,
						410,
						72,
						22
					],
					"text": "pack 0 0.",
					"outlettype": [
						""
					]
				}
			},
			{
				"box": {
					"id": "obj-25",
					"maxclass": "newobj",
					"numinlets": 1,
					"numoutlets": 1,
					"patching_rect": [
						530,
						445,
						28,
						22
					],
					"text": "t l",
					"outlettype": [
						""
					]
				}
			},
			{
				"box": {
					"id": "obj-26",
					"maxclass": "live.menu",
					"numinlets": 1,
					"numoutlets": 3,
					"patching_rect": [
						30,
						290,
						100,
						15
					],
					"outlettype": [
						"",
						"",
						"float"
					],
					"parameter_enable": 1,
					"presentation": 1,
					"presentation_rect": [
						10,
						38,
						100,
						15
					],
					"saved_attribute_attributes": {
						"valueof": {
							"parameter_enum": [
								"MPE",
								"Mono PB"
							],
							"parameter_initial": [
								0
							],
							"parameter_initial_enable": 1,
							"parameter_linknames": 1,
							"parameter_longname": "Mode",
							"parameter_mmax": 1,
							"parameter_shortname": "Mode",
							"parameter_type": 2
						}
					},
					"varname": "Mode"
				}
			},
			{
				"box": {
					"id": "obj-27",
					"maxclass": "newobj",
					"numinlets": 1,
					"numoutlets": 1,
					"patching_rect": [
						30,
						325,
						105,
						22
					],
					"text": "prepend set_mode",
					"outlettype": [
						""
					]
				}
			},
			{
				"box": {
					"id": "obj-28",
					"maxclass": "live.dial",
					"numinlets": 1,
					"numoutlets": 2,
					"patching_rect": [
						180,
						280,
						44,
						48
					],
					"outlettype": [
						"",
						"float"
					],
					"parameter_enable": 1,
					"presentation": 1,
					"presentation_rect": [
						130,
						23,
						44,
						48
					],
					"saved_attribute_attributes": {
						"valueof": {
							"parameter_initial": [
								48
							],
							"parameter_initial_enable": 1,
							"parameter_linknames": 1,
							"parameter_longname": "Bend Range",
							"parameter_mmax": 96.0,
							"parameter_mmin": 1.0,
							"parameter_shortname": "PB Range",
							"parameter_type": 1,
							"parameter_unitstyle": 9
						}
					},
					"varname": "Bend Range"
				}
			},
			{
				"box": {
					"id": "obj-29",
					"maxclass": "newobj",
					"numinlets": 1,
					"numoutlets": 1,
					"patching_rect": [
						180,
						340,
						130,
						22
					],
					"text": "prepend set_bend_range",
					"outlettype": [
						""
					]
				}
			}
		],
		"lines": [
			{
				"patchline": {
					"source": [
						"obj-4",
						0
					],
					"destination": [
						"obj-5",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-5",
						0
					],
					"destination": [
						"obj-6",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-5",
						1
					],
					"destination": [
						"obj-7",
						1
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-5",
						2
					],
					"destination": [
						"obj-7",
						2
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-5",
						3
					],
					"destination": [
						"obj-7",
						3
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-5",
						4
					],
					"destination": [
						"obj-7",
						4
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-5",
						5
					],
					"destination": [
						"obj-7",
						5
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-5",
						6
					],
					"destination": [
						"obj-7",
						6
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-6",
						0
					],
					"destination": [
						"obj-8",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-7",
						0
					],
					"destination": [
						"obj-8",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-9",
						0
					],
					"destination": [
						"obj-10",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-10",
						0
					],
					"destination": [
						"obj-11",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-11",
						0
					],
					"destination": [
						"obj-12",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-12",
						0
					],
					"destination": [
						"obj-19",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-10",
						1
					],
					"destination": [
						"obj-13",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-13",
						0
					],
					"destination": [
						"obj-14",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-14",
						0
					],
					"destination": [
						"obj-15",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-15",
						2
					],
					"destination": [
						"obj-16",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-16",
						0
					],
					"destination": [
						"obj-17",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-17",
						0
					],
					"destination": [
						"obj-19",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-18",
						0
					],
					"destination": [
						"obj-19",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-20",
						0
					],
					"destination": [
						"obj-19",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-19",
						3
					],
					"destination": [
						"obj-21",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-21",
						0
					],
					"destination": [
						"obj-22",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-21",
						1
					],
					"destination": [
						"obj-23",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-22",
						0
					],
					"destination": [
						"obj-24",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-23",
						0
					],
					"destination": [
						"obj-24",
						1
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-24",
						0
					],
					"destination": [
						"obj-25",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-25",
						0
					],
					"destination": [
						"obj-6",
						1
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-26",
						0
					],
					"destination": [
						"obj-27",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-27",
						0
					],
					"destination": [
						"obj-6",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-28",
						0
					],
					"destination": [
						"obj-29",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-29",
						0
					],
					"destination": [
						"obj-6",
						0
					]
				}
			}
		],
		"dependency_cache": [],
		"autosave": 0,
		"openrect": [
			0.0,
			0.0,
			400.0,
			170.0
		],
		"is_mpe": 1,
		"title": "Tanghim Receiver",
		"latency": 0,
		"project": {
			"version": 1,
			"creationdate": 3590052786,
			"modificationdate": 3590052786,
			"viewrect": [
				0.0,
				0.0,
				300.0,
				500.0
			],
			"autoorganize": 1,
			"hideprojectwindow": 1,
			"showdependencies": 1,
			"autolocalize": 0,
			"contents": {
				"patchers": {},
				"code": {}
			},
			"layout": {},
			"searchpath": {},
			"detailsvisible": 0,
			"amxdtype": 1835887981,
			"readonly": 0,
			"devpathtype": 0,
			"devpath": ".",
			"sortmode": 0,
			"viewmode": 0
		}
	}
}