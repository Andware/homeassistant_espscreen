// What the wizards of Events understand of the entity someone picks to start an automation (model/automation.ts): a
// helper or sensor that turns on, a doorbell Home Assistant knows as one, any other event entity, a button; and the
// trigger each one gets, as Home Assistant's own editor writes it.
import { describe, expect, it } from "vitest";
import { automationYaml, title, trigger, triggerKind } from "../src/model/automation";
import { doorbellAutomation, triggerKind as doorbellKind, type DoorbellReference } from "../src/model/doorbell-yaml";

describe("what starts an automation", () => {
  it("reads a boolean, a sensor and a switch as turning on", () => {
    for (const entity of ["input_boolean.someone_at_the_door", "binary_sensor.porch_visitor", "switch.bell"]) {
      expect(triggerKind(entity)).toBe("on");
      expect(trigger(entity, "on")).toEqual({ trigger: "state", entity_id: entity, from: "off", to: "on" });
    }
  });

  it("reads a doorbell Home Assistant knows as ringing, and any other event as firing", () => {
    const known = { "event.front_door_doorbell": "doorbell" as const };
    expect(triggerKind("event.front_door_doorbell", known)).toBe("doorbell");
    expect(trigger("event.front_door_doorbell", "doorbell")).toEqual({ trigger: "doorbell.rang", target: { entity_id: "event.front_door_doorbell" } });
    // A doorbell's event entity picked without the suggestion still starts it: every new state is a ring.
    expect(triggerKind("event.front_door_doorbell")).toBe("event");
    expect(trigger("event.remote_button", "event")).toEqual({ trigger: "state", entity_id: "event.remote_button", not_from: ["unavailable", "unknown"] });
  });

  it("reads a button as pressed, and leaves what cannot start one out", () => {
    expect(triggerKind("input_button.ring")).toBe("press");
    expect(triggerKind("button.ring")).toBe("press");
    for (const entity of ["sensor.temperature", "light.hall", "camera.front_door"]) expect(triggerKind(entity)).toBeNull();
  });

  it("calls every automation Tessera: <name>, once", () => {
    expect(title("Front door")).toBe("Tessera: Front door");
    expect(title("tessera: Front door")).toBe("Tessera: Front door");
  });
});

describe("the doorbell's automation", () => {
  const reference: DoorbellReference = { show: "esp_screens_doorbell", timeout: { default: 60, min: 10, max: 600 },
    suggestions: [{ trigger: "binary_sensor.porch_visitor", kind: "on", camera: "camera.porch", name: "Porch Visitor" }] };

  it("follows what the add-on said of the doorbell, and the screens and timeout chosen", () => {
    expect(doorbellKind(reference, "binary_sensor.porch_visitor")).toBe("on");
    const automation = doorbellAutomation(reference, { trigger: "binary_sensor.porch_visitor", camera: "camera.porch", name: "Porch",
      screens: ["hallway", "kitchen"], timeout: 90 });
    expect(automation.alias.startsWith("Tessera: ")).toBe(true);
    expect(automation.triggers).toEqual([{ trigger: "state", entity_id: "binary_sensor.porch_visitor", from: "off", to: "on" }]);
    expect(automation.actions).toEqual([{ event: "esp_screens_doorbell", event_data: { camera: "camera.porch", screen: ["hallway", "kitchen"], timeout: 90 } }]);
    expect(automationYaml(automation)).toContain("    from: \"off\"\n    to: \"on\"");
  });
});
