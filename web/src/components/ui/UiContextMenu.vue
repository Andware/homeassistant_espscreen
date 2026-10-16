<script setup lang="ts">
// A menu on the right mouse button, or a finger held still (Reka UI): the thing it belongs to is the slot, the items
// are UiMenuItem, UiMenuLabel and UiMenuSeparator in the `menu` slot, as in UiMenu.
import { ContextMenuContent, ContextMenuPortal, ContextMenuRoot, ContextMenuTrigger } from "reka-ui";
defineProps<{ width?: string; disabled?: boolean }>();
const open = defineModel<boolean>("open", { default: false });
</script>

<template>
  <ContextMenuRoot v-model:open="open" :modal="false">
    <ContextMenuTrigger as-child :disabled="disabled"><slot /></ContextMenuTrigger>
    <ContextMenuPortal>
      <ContextMenuContent class="ui-popover ui-menu" :style="width ? { minWidth: width } : undefined" :collision-padding="8">
        <slot name="menu" />
      </ContextMenuContent>
    </ContextMenuPortal>
  </ContextMenuRoot>
</template>
