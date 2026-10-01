PYTHON = python3

OSM_DIR = data/osm
MAP_DIR = data/map
OUTPUT_DIR = data/output

.PHONY: map maps clean-map clean-maps


# ========================================
# 生成单张地图
#
# 使用：
# make map MAP=weihai
#
# 输入：
# data/osm/weihai.osm
#
# 输出：
# data/map/weihai.txt
# data/output/weihai.png
# ========================================

map:
	$(PYTHON) tools/osm-txt.py \
		$(OSM_DIR)/$(MAP).osm \
		$(MAP_DIR)/$(MAP).txt

	$(PYTHON) tools/drawgraph.py \
		$(MAP_DIR)/$(MAP).txt \
		$(OUTPUT_DIR)/$(MAP).png


# ========================================
# 生成全部地图
#
# 使用：
# make maps
# ========================================

maps:
	for file in $(OSM_DIR)/*.osm; do \
		name=$$(basename $$file .osm); \
		echo "========================================"; \
		echo "正在处理：$$name"; \
		echo "========================================"; \
		$(PYTHON) tools/osm-txt.py \
			$$file \
			$(MAP_DIR)/$$name.txt; \
		$(PYTHON) tools/drawgraph.py \
			$(MAP_DIR)/$$name.txt \
			$(OUTPUT_DIR)/$$name.png; \
	done


# ========================================
# 删除单张地图生成的数据
#
# 使用：
# make clean-map MAP=weihai
#
# 不会删除 .osm 原始地图
# ========================================

clean-map:
	rm -f $(MAP_DIR)/$(MAP).txt
	rm -f $(OUTPUT_DIR)/$(MAP).png


# ========================================
# 删除全部生成的数据
#
# 使用：
# make clean-maps
#
# 不会删除 data/osm/ 中的 .osm
# ========================================

clean-maps:
	rm -f $(MAP_DIR)/*.txt
	rm -f $(OUTPUT_DIR)/*.png