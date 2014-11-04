#include <mach/sys_config.h>
#include "axp-regu.h"
#include "axp81x/axp81x-regu.h"

int axp_regu_fetch_sysconfig_para(char * pmu_type, struct axp_regu_data *axpxx_data, struct axp_reg_init *axp_init_data)
{
	int ldo_count = 0, ldo_index = 0;
	char name[32] = {0};
	script_item_u val;
	script_item_value_type_e type;
	int num = 0, i = 0;
	struct axp_consumer_supply (*consumer_supply)[20] = NULL;
	struct axp_consumer_supply (*consumer_supply_base)[20] = NULL;
	consumer_supply_base = &(axpxx_data->axp_ldo1_data);

	type = script_get_item(pmu_type, "ldo_count", &val);
	if (SCIRPT_ITEM_VALUE_TYPE_INT != type) {
		printk("fetch axp regu tree from sysconfig failed\n");
		return -1;
	} else
		ldo_count = val.val;

	for (ldo_index = 1; ldo_index <= ldo_count; ldo_index++) {
		sprintf(name, "ldo%d", ldo_index);
		type = script_get_item(pmu_type, name, &val);
		if (SCIRPT_ITEM_VALUE_TYPE_STR != type) {
			printk("get %s from sysconfig failed\n", name);
			continue;
		}
		consumer_supply = consumer_supply_base + (ldo_index - 1);
		num = sscanf(val.str, "%s %s %s %s %s %s %s %s %s %s %s %s %s %s %s %s %s %s %s %s",
			((*consumer_supply)+0)->supply, ((*consumer_supply)+1)->supply, ((*consumer_supply)+2)->supply,
			((*consumer_supply)+3)->supply, ((*consumer_supply)+4)->supply, ((*consumer_supply)+5)->supply,
			((*consumer_supply)+6)->supply, ((*consumer_supply)+7)->supply, ((*consumer_supply)+8)->supply,
			((*consumer_supply)+9)->supply, ((*consumer_supply)+10)->supply, ((*consumer_supply)+11)->supply,
			((*consumer_supply)+12)->supply, ((*consumer_supply)+13)->supply, ((*consumer_supply)+14)->supply,
			((*consumer_supply)+15)->supply, ((*consumer_supply)+16)->supply, ((*consumer_supply)+17)->supply,
			((*consumer_supply)+18)->supply, ((*consumer_supply)+19)->supply);
		if (num <= -1) {
			printk("parse ldo%d from sysconfig failed\n", ldo_index);
			return -1;
		} else {
			(*(axp_init_data+(ldo_index-1))).axp_reg_init_data.num_consumer_supplies = num;
			DBG_PSY_MSG(DEBUG_REGU, "%s: num = %d\n", __func__, num);
			for (i = 0; i < num; i++) {
				if (i == (num-1)) {
					DBG_PSY_MSG(DEBUG_REGU, "%s  \n", ((*consumer_supply)+i)->supply);
				} else {
					DBG_PSY_MSG(DEBUG_REGU, "%s  ", ((*consumer_supply)+i)->supply);
				}
			}
		}
	}
	return 0;
}

