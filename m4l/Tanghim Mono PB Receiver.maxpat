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
		"devicewidth": 99.0,
		"description": "MTS-ESP Mono PB Receiver",
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
						45,
						475,
						292,
						29
					],
					"presentation": 1,
					"presentation_rect": [
						0.0,
						0.0,
						100.0,
						29.0
					],
					"fontname": "Cairo Black",
					"text": "\u062a\u0646\u063a\u064a\u0645",
					"textcolor": [
						0.0,
						0.0,
						0.0,
						1.0
					],
					"textjustification": 1
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
						460,
						178,
						21
					],
					"presentation": 1,
					"presentation_rect": [
						0.0,
						30.0,
						100.0,
						35.0
					],
					"presentation_linecount": 2,
					"fontname": "Ableton Sans Medium",
					"fontsize": 12.0,
					"text": "Tanghim Mono PB Receiver",
					"textcolor": [
						0.0,
						0.0,
						0.0,
						1.0
					],
					"textjustification": 1
				}
			},
			{
				"box": {
					"id": "obj-3",
					"maxclass": "newobj",
					"numinlets": 1,
					"numoutlets": 1,
					"patching_rect": [
						20,
						60,
						100,
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
					"id": "obj-4",
					"maxclass": "newobj",
					"numinlets": 1,
					"numoutlets": 8,
					"patching_rect": [
						20,
						95,
						300,
						22
					],
					"text": "midiparse @hires 2",
					"outlettype": [
						"",
						"",
						"",
						"int",
						"int",
						"int",
						"int",
						""
					]
				}
			},
			{
				"box": {
					"id": "obj-5",
					"maxclass": "newobj",
					"numinlets": 1,
					"numoutlets": 2,
					"patching_rect": [
						20,
						130,
						100,
						22
					],
					"text": "unpack 0 0",
					"outlettype": [
						"int",
						"int"
					]
				}
			},
			{
				"box": {
					"id": "obj-6",
					"maxclass": "newobj",
					"numinlets": 1,
					"numoutlets": 2,
					"patching_rect": [
						20,
						165,
						60,
						22
					],
					"text": "t i i",
					"outlettype": [
						"int",
						"int"
					]
				}
			},
			{
				"box": {
					"id": "obj-7",
					"maxclass": "newobj",
					"numinlets": 1,
					"numoutlets": 4,
					"patching_rect": [
						100,
						165,
						140,
						22
					],
					"text": "MTS-ESP.mtof",
					"outlettype": [
						"float",
						"float",
						"float",
						"int"
					]
				}
			},
			{
				"box": {
					"id": "obj-8",
					"maxclass": "newobj",
					"numinlets": 1,
					"numoutlets": 1,
					"patching_rect": [
						260,
						165,
						110,
						22
					],
					"text": "expr $f1 * 100.",
					"outlettype": [
						""
					]
				}
			},
			{
				"box": {
					"id": "obj-9",
					"maxclass": "newobj",
					"numinlets": 2,
					"numoutlets": 1,
					"patching_rect": [
						380,
						165,
						60,
						22
					],
					"text": "int",
					"outlettype": [
						"int"
					]
				}
			},
			{
				"box": {
					"id": "obj-10",
					"maxclass": "newobj",
					"numinlets": 2,
					"numoutlets": 1,
					"patching_rect": [
						440,
						100,
						60,
						22
					],
					"text": "int",
					"outlettype": [
						"int"
					]
				}
			},
			{
				"box": {
					"id": "obj-11",
					"maxclass": "newobj",
					"numinlets": 1,
					"numoutlets": 2,
					"patching_rect": [
						440,
						60,
						60,
						22
					],
					"text": "t b i",
					"outlettype": [
						"bang",
						"int"
					]
				}
			},
			{
				"box": {
					"id": "obj-12",
					"maxclass": "newobj",
					"numinlets": 2,
					"numoutlets": 1,
					"patching_rect": [
						150,
						200,
						60,
						22
					],
					"text": "* 1",
					"outlettype": [
						"int"
					]
				}
			},
			{
				"box": {
					"id": "obj-13",
					"maxclass": "newobj",
					"numinlets": 2,
					"numoutlets": 4,
					"patching_rect": [
						20,
						235,
						80,
						22
					],
					"text": "poly 1 1",
					"outlettype": [
						"int",
						"int",
						"int",
						""
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
						20,
						270,
						60,
						22
					],
					"text": "int",
					"outlettype": [
						"int"
					]
				}
			},
			{
				"box": {
					"id": "obj-15",
					"maxclass": "newobj",
					"numinlets": 1,
					"numoutlets": 2,
					"patching_rect": [
						20,
						305,
						70,
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
					"id": "obj-16",
					"maxclass": "newobj",
					"numinlets": 3,
					"numoutlets": 0,
					"patching_rect": [
						20,
						345,
						90,
						22
					],
					"text": "noteout"
				}
			},
			{
				"box": {
					"id": "obj-17",
					"maxclass": "newobj",
					"numinlets": 1,
					"numoutlets": 1,
					"patching_rect": [
						130,
						310,
						70,
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
					"id": "obj-18",
					"maxclass": "message",
					"numinlets": 0,
					"numoutlets": 1,
					"patching_rect": [
						130,
						335,
						25,
						22
					],
					"text": "1"
				}
			},
			{
				"box": {
					"id": "obj-19",
					"maxclass": "newobj",
					"numinlets": 0,
					"numoutlets": 1,
					"patching_rect": [
						380,
						305,
						90,
						22
					],
					"text": "receive pbRange",
					"outlettype": [
						""
					]
				}
			},
			{
				"box": {
					"id": "obj-20",
					"maxclass": "newobj",
					"numinlets": 0,
					"numoutlets": 1,
					"patching_rect": [
						480,
						305,
						110,
						22
					],
					"text": "receive userPbRange",
					"outlettype": [
						""
					]
				}
			},
			{
				"box": {
					"id": "obj-21",
					"maxclass": "newobj",
					"numinlets": 4,
					"numoutlets": 1,
					"patching_rect": [
						290,
						340,
						360,
						22
					],
					"text": "expr int(8192 + ($f1 / ($f2 * 100.)) * 8191 + ($i3 * $f4 / $f2))",
					"outlettype": [
						""
					]
				}
			},
			{
				"box": {
					"id": "obj-22",
					"maxclass": "newobj",
					"numinlets": 3,
					"numoutlets": 1,
					"patching_rect": [
						290,
						375,
						120,
						22
					],
					"text": "clip 0 16383",
					"outlettype": [
						""
					]
				}
			},
			{
				"box": {
					"id": "obj-23",
					"maxclass": "newobj",
					"numinlets": 1,
					"numoutlets": 0,
					"patching_rect": [
						290,
						415,
						80,
						22
					],
					"text": "xbendout 1"
				}
			},
			{
				"box": {
					"id": "obj-24",
					"maxclass": "newobj",
					"numinlets": 1,
					"numoutlets": 0,
					"patching_rect": [
						290,
						450,
						80,
						22
					],
					"text": "midiout"
				}
			},
			{
				"box": {
					"id": "obj-25",
					"maxclass": "comment",
					"numinlets": 1,
					"numoutlets": 0,
					"patching_rect": [
						180,
						250,
						80,
						18
					],
					"presentation": 1,
					"presentation_rect": [
						10.0,
						80.0,
						80.0,
						18.0
					],
					"fontname": "Ableton Sans Medium",
					"fontsize": 10.0,
					"text": "PB Range",
					"textcolor": [
						0.0,
						0.0,
						0.0,
						1.0
					],
					"textjustification": 1
				}
			},
			{
				"box": {
					"id": "obj-26",
					"maxclass": "live.dial",
					"numinlets": 1,
					"numoutlets": 2,
					"patching_rect": [
						180,
						280,
						27,
						48
					],
					"outlettype": [
						"",
						"float"
					],
					"parameter_enable": 1,
					"presentation": 1,
					"presentation_rect": [
						20.0,
						90.0,
						60.0,
						48.0
					],
					"saved_attribute_attributes": {
						"valueof": {
							"parameter_initial": [
								2
							],
							"parameter_initial_enable": 1,
							"parameter_linknames": 1,
							"parameter_longname": "PB Range",
							"parameter_mmax": 96.0,
							"parameter_mmin": 1.0,
							"parameter_shortname": " ",
							"parameter_type": 1,
							"parameter_unitstyle": 9
						}
					},
					"varname": "PB Range"
				}
			},
			{
				"box": {
					"id": "obj-27",
					"maxclass": "newobj",
					"numinlets": 1,
					"numoutlets": 0,
					"patching_rect": [
						20,
						435,
						90,
						22
					],
					"text": "send pbRange"
				}
			},
			{
				"box": {
					"id": "obj-28",
					"maxclass": "newobj",
					"numinlets": 1,
					"numoutlets": 0,
					"patching_rect": [
						20,
						460,
						110,
						22
					],
					"text": "send userPbRange"
				}
			},
			{
				"box": {
					"id": "obj-29",
					"maxclass": "newobj",
					"numinlets": 0,
					"numoutlets": 1,
					"patching_rect": [
						600,
						60,
						160,
						22
					],
					"text": "js registry_monopb.js",
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
						"obj-3",
						0
					],
					"destination": [
						"obj-4",
						0
					]
				}
			},
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
						"obj-6",
						1
					],
					"destination": [
						"obj-7",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-7",
						2
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
						"obj-8",
						0
					],
					"destination": [
						"obj-9",
						1
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-4",
						5
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
						1
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
						"obj-11",
						0
					],
					"destination": [
						"obj-9",
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
						"obj-12",
						1
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-7",
						3
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
						"obj-13",
						1
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
						"obj-13",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-13",
						1
					],
					"destination": [
						"obj-14",
						1
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
						"obj-15",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-15",
						1
					],
					"destination": [
						"obj-9",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-15",
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
						"obj-13",
						2
					],
					"destination": [
						"obj-16",
						1
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
						"obj-16",
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
						"obj-18",
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
						"obj-16",
						2
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
						"obj-21",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-19",
						0
					],
					"destination": [
						"obj-21",
						1
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
						"obj-21",
						2
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
						"obj-21",
						3
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
						"obj-22",
						0
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
						"obj-23",
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
						"obj-26",
						0
					],
					"destination": [
						"obj-28",
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
			99.0,
			169.0
		],
		"title": "Tanghim Mono PB Receiver",
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