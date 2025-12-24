




(function($scope, $as) {
"use strict";

function FAIL(msg) { throw new Error("[BASE] " + msg); }

const exp = $scope.exp ? $scope.exp : require("./exp.js");


const base = $scope[$as] = Object.create(null);

base.exp = exp;

function dict(src) {
  const dst = Object.create(null);
  if (src)
    Object.assign(dst, src);
  return dst;
}
base.dict = dict;
const NONE = base.NONE = Object.freeze(dict());




const Symbols = Object.freeze({
  Commutative: '~'
});
base.Symbols = Symbols;





const Parsing = {
  
  isImplicit: function(s) { return s.startsWith("<") && s.endsWith(">"); },

  
  clearImplicit: function(s) { return s.substring(1, s.length - 1); },

  
  isOptional: function(s) { return s.startsWith("{") && s.endsWith("}"); },

  
  clearOptional: function(s) { return s.substring(1, s.length - 1); },

  
  isCommutative: function(s) { return s.length > 0 && s.charAt(0) === Symbols.Commutative; },

  
  clearCommutative: function(s) { return s.substring(1); },

  
  
  
  matchClosingChar: function(s, from) {
    const len = s.length;
    const opening = s.charCodeAt(from);
    const closing = opening === 40  ? 31  :    
                    opening === 60  ? 62  :    
                    opening === 91  ? 93  :    
                    opening === 123 ? 125 : 0; 

    let i = from;
    let pending = 1;
    do {
      if (++i >= len)
        break;

      const c = s.charCodeAt(i);
      pending += Number(c === opening);
      pending -= Number(c === closing);
    } while (pending);

    return i;
  },

  
  
  
  
  
  splitOperands: function(s) {
    const result = [];

    s = s.trim();
    if (!s)
      return result;

    let start = 0;
    let i = 0;
    let c = "";

    for (;;) {
      if (i === s.length || (c = s[i]) === ",") {
        const op = s.substring(start, i).trim();
        if (!op)
          FAIL(`Found empty operand in '${s}'`);

        result.push(op);
        if (i === s.length)
          return result;

        start = ++i;
        continue;
      }

      if ((c === "<" || c === ">") && i != start) {
        i++;
        continue;
      }

      if (c === "[" || c === "{" || c === "(" || c === "<")
        i = base.Parsing.matchClosingChar(s, i);
      else
        i++;
    }
  }
}
base.Parsing = Parsing;




class MapUtils {
  static cloneExcept(map, except) {
    if (typeof except === "string") {
      const key = except;
      except = Object.create(null);
      except[key] = true;
    }

    const out = Object.create(null);
    for (let k in map) {
      if (k in except)
        continue
      out[k] = map[k];
    }
    return out;
  }

  static mapFromArray(array) {
    const out = Object.create(null);
    for (let k of array) {
      out[k] = true;
    }
    return out;
  }
};
base.MapUtils = MapUtils;




const OperandFlags = Object.freeze({
  Optional   : 0x00000001,
  Implicit   : 0x00000002,
  Commutative: 0x00000004,
  ZExt       : 0x00000008,
  ReadAccess : 0x00000010,
  WriteAccess: 0x00000020
});
base.OperandFlags = OperandFlags;

class Operand {
  constructor() {
    this.type = "";              
    this.data = "";              
    this.flags = 0;

    this.reg = "";               
    this.mem = "";               
    this.imm = 0;                
    this.rel = 0;                

    this.restrict = "";          
    this.read = false;           
    this.write = false;          

    this.regType = "";           
    this.regIndexRel = 0;        
    this.memSize = -1;           
    this.immSign = "";           // Immediate sign (any / signed / unsigned).
    this.immValue = null;        // Immediate value - `null` or `1` (only used by shift/rotate instructions).

    this.rwxIndex = -1;          // Read/Write (RWX) index.
    this.rwxWidth = -1;          // Read/Write (RWX) width.
  }

  _getFlag(flag) {
    return (this.flags & flag) != 0;
  }

  _setFlag(flag, value) {
    this.flags = (this.flags & ~flag) | (value ? flag : 0);
    return this;
  }

  get optional() { return this._getFlag(OperandFlags.Optional); }
  set optional(value) { this._setFlag(OperandFlags.Optional, value); }

  get implicit() { return this._getFlag(OperandFlags.Implicit); }
  set implicit(value) { this._setFlag(OperandFlags.Implicit, value); }

  get commutative() { return this._getFlag(OperandFlags.Commutative); }
  set commutative(value) { this._setFlag(OperandFlags.Commutative, value); }

  get zext() { return this._getFlag(OperandFlags.ZExt); }
  set zext(value) { this._setFlag(OperandFlags.ZExt, value); }

  toString() { return this.data; }

  isReg() { return !!this.reg && this.type !== "reg-list"; }
  isMem() { return !!this.mem; }
  isImm() { return !!this.imm; }
  isRel() { return !!this.rel; }

  isRegMem() { return this.reg && this.mem; }
  isRegOrMem() { return !!this.reg || !!this.mem; }

  isRegList() { return this.type === "reg-list" }
  isPartialOp() { return false; }
}
base.Operand = Operand;

// asmdb.base.Instruction
// ======================

// Defines interface and properties that each architecture dependent instruction
// must provide even if that particular architecture doesn't use that feature(s).
class Instruction {
  constructor(db) {
    Object.defineProperty(this, "db", { value: db });

    this.name = "";            // Instruction name.
    this.arch = "ANY";         // Architecture.
    this.encoding = "";        // Encoding type.
    this.operands = [];        // Instruction operands.

    this.implicit = 0;         // Indexes of all implicit operands (registers / memory).
    this.commutative = 0;      // Indexes of all commutative operands.

    this.opcodeString = "";    // Instruction opcode as specified in manual.
    this.opcodeValue = 0;      // Instruction opcode as number (arch dependent).
    this.fields = dict();      // Information about each opcode field (arch dependent).
    this.operations = dict();  // Operations the instruction performs.

    this.io = dict();          // Instruction input / output (CPU flags, states, and other registers).
    this.ext = dict();         // ISA extensions required by the instruction.
    this.category = dict();    // Instruction categories.

    this.specialRegs = dict(); // Information about read/write to special registers.

    this.alt = false;          // This is an alternative form, not needed to create a signature.
    this.volatile = false;     // Instruction is volatile and should not be reordered.
    this.control = "none";     // Control flow type (none by default).
    this.privilege = "";       // Privilege-level required to execute the instruction.
    this.aliasOf = "";         // Instruction is an alias of another instruction
  }

  get extArray() {
    const out = Object.keys(this.ext);
    out.sort();
    return out;
  }

  get operandCount() {
    return this.operands.length;
  }

  get minimumOperandCount() {
    const count = this.operands.length;
    for (let i = 0; i < count; i++) {
      if (this.operands[i].optional) {
        return i;
      }
    }
    return count
  }

  _assignAttribute(key, value) {
    switch (key) {
      case "ext":
      case "io":
      case "category":
        return this._combineAttribute(key, value);

      default:
        if (typeof this[key] === undefined)
          FAIL(`Cannot assign ${key}=${value}`);
        this[key] = value;
        break;
    }
  }

  _combineAttribute(key, value) {
    if (typeof value === "string")
      value = value.split(" ");

    if (Array.isArray(value)) {
      for (let v of value) {
        let pKeys = v;
        let pValue = true;

        const i = v.indexOf("=");
        if (i !== -1) {
          pValue = v.substring(i + 1);
          pKeys = v.substring(0, i).trim();
        }

        for (let pk of pKeys.trim().split("|").map(function(s) { return s.trim(); })) {
          this[key][pk] = pValue;
        }
      }
    }
    else {
      for (let k in value)
        this[key][k] = value[k];
    }
  }

  _updateOperandsInfo() {
    this.implicit = 0;
    this.commutative = 0;

    for (let i = 0; i < this.operands.length; i++) {
      const op = this.operands[i];

      if (op.implicit) this.implicit |= (1 << i);
      if (op.commutative) this.commutative |= (1 << i);
    }
  }

  isAlias() { return !!this.aliasOf; }
  isCommutative() { return this.commutative !== 0; }

  hasImplicit() { return this.implicit !== 0; }

  hasAttribute(name, matchValue) {
    const value = this[name];
    if (value === undefined)
      return false;

    if (matchValue === undefined)
      return true;

    return value === matchValue;
  }

  report(msg) {
    console.log(`${this}: ${msg}`);
  }

  toString() {
    return `${this.name} ${this.operands.join(", ")}`;
  }
}
base.Instruction = Instruction;

// asmdb.base.InstructionGroup
// ===========================

// Instruction group is simply array of function that has some additional
// functionality.
class InstructionGroup extends Array {
  constructor() {
    super();

    if (arguments.length === 1) {
      const a = arguments[0];
      if (Array.isArray(a)) {
        for (let i = 0; i < a.length; i++)
          this.push(a[i]);
      }
    }
  }

  unionCpuFeatures(name) {
    const result = dict();
    for (let i = 0; i < this.length; i++) {
      const instruction = this[i];
      const features = instruction.ext;
      for (let k in features)
        result[k] = features[k];
    }
    return result;
  }

  checkAttribute(key, value) {
    let n = 0;
    for (let i = 0; i < this.length; i++)
      n += Number(this[i][key] === value);
    return n;
  }
}
base.InstructionGroup = InstructionGroup;

const EmptyInstructionGroup = Object.freeze(new InstructionGroup());

// asmdb.base.ISA
// ==============

class ISA {
  constructor() {
    this._instructions = null;           // Instruction array (contains all instructions).
    this._instructionNames = null;       // Instruction names (sorted), regenerated when needed.
    this._instructionMap = dict();       // Instruction name to `Instruction[]` mapping.
    this._aliases = dict();              // Instruction aliases.
    this._aliasMap = dict();             // Instruction aliases.
    this._cpuLevels = dict();            // Architecture versions.
    this._extensions = dict();           // Architecture extensions.
    this._attributes = dict();           // Instruction attributes.
    this._specialRegs = dict();          // Special registers.
    this._shortcuts = dict();            // Shortcuts used by instructions metadata.
    this.stats = {
      instructions : 0,                  // Number of all instructions.
      groups: 0                          // Number of grouped instructions (having unique name).
    };
  }

  get instructions() {
    let array = this._instructions;
    if (array === null) {
      array = [];
      const map = this.instructionMap;
      const names = this.instructionNames;
      for (let i = 0; i < names.length; i++)
        array.push.apply(array, map[names[i]]);
      this._instructions = array;
    }
    return array;
  }

  get instructionNames() {
    let names = this._instructionNames;
    if (names === null) {
      names = Object.keys(this._instructionMap);
      names.sort();
      this._instructionNames = names;
    }
    return names;
  }

  get instructionMap() { return this._instructionMap; }
  get aliases() { return this._aliasMap; }
  get cpuLevels() { return this._cpuLevels; }
  get extensions() { return this._extensions; }
  get attributes() { return this._attributes; }
  get specialRegs() { return this._specialRegs; }
  get shortcuts() { return this._shortcuts; }

  query(args, copy) {
    if (typeof args !== "object" || !args || Array.isArray(args))
      return this._queryByName(args, copy);

    const filter = args.filter;
    if (filter)
      copy = false;

    let result = this._queryByName(args.name, copy);
    if (filter)
      result = result.filter(filter, args.filterThis);

    return result;
  }

  aliasData(name) {
    return this._aliases[name] || null;
  }

  _queryByName(name, copy) {
    let result = EmptyInstructionGroup;
    const map = this._instructionMap;

    if (typeof name === "string") {
      const instructions = map[name];
      if (instructions) result = instructions;
      return copy ? result.slice() : result;
    }

    if (Array.isArray(name)) {
      const names = name;
      for (let i = 0; i < names.length; i++) {
        const instructions = map[names[i]];
        if (!instructions) continue;

        if (result === EmptyInstructionGroup)
          result = new InstructionGroup();

        for (let j = 0; j < instructions.length; j++)
          result.push(instructions[j]);
      }
      return result;
    }

    result = this.instructions;
    return copy ? result.slice() : result;
  }

  forEachGroup(cb, thisArg) {
    const map = this._instructionMap;
    const names = this.instructionNames;

    for (let i = 0; i < names.length; i++) {
      const name = names[i];
      cb.call(thisArg, name, map[name]);
    }

    return this;
  }

  addData(data) {
    if (typeof data !== "object" || !data)
      FAIL("ISA.addData(): data argument must be object");

    if (data.cpuLevels) this._addCpuLevels(data.cpuLevels);
    if (data.specialRegs) this._addSpecialRegs(data.specialRegs);
    if (data.shortcuts) this._addShortcuts(data.shortcuts);
    if (data.instructions) this._addInstructions(data.instructions);
    if (data.aliases) this._addAliases(data.aliases);
    if (data.postproc) this._postProc(data.postproc);
  }

  _postProc(groups) {
    for (let group of groups) {
      for (let iRule of group.instructions) {
        const names = iRule.name.split(" ");
        for (let name of names) {
          const instructions = this._instructionMap[name];
          if (!instructions)
            FAIL(`Instruction ${name} referenced by '${group.group}' group doesn't exist`);

          for (let k in iRule) {
            if (k === "name" || k === "data")
              continue;
            for (let instruction of instructions) {
              instruction._assignAttribute(k, iRule[k]);
            }
          }
        }
      }
    }
  }

  _addCpuLevels(items) {
    if (!Array.isArray(items))
      FAIL("Property 'cpuLevels' must be array");

    for (let i = 0; i < items.length; i++) {
      const item = items[i];
      const name = item.name;

      const obj = {
        name: name
      };

      this._cpuLevels[name] = obj;
    }
  }

  _addExtensions(items) {
    if (!Array.isArray(items))
      FAIL("Property 'extensions' must be array");

    for (let i = 0; i < items.length; i++) {
      const item = items[i];
      const name = item.name;

      const obj = {
        name: name,
        from: item.from || ""
      };

      this._extensions[name] = obj;
    }
  }

  _addAttributes(items) {
    if (!Array.isArray(items))
      FAIL("Property 'attributes' must be array");

    for (let i = 0; i < items.length; i++) {
      const item = items[i];
      const name = item.name;
      const type = item.type;

      if (!/^(?:flag|string|string\[\])$/.test(type))
        FAIL(`Unknown attribute type '${type}'`);

      const obj = {
        name: name,
        type: type,
        doc : item.doc || ""
      };

      this._attributes[name] = obj;
    }
  }

  _addSpecialRegs(items) {
    if (!Array.isArray(items))
      FAIL("Property 'specialRegs' must be array");

    for (let i = 0; i < items.length; i++) {
      const item = items[i];
      const name = item.name;

      const obj = {
        name : name,
        group: item.group || name,
        doc  : item.doc || ""
      };

      this._specialRegs[name] = obj;
    }
  }

  _addShortcuts(items) {
    if (!Array.isArray(items))
      FAIL("Property 'shortcuts' must be array");

    for (let i = 0; i < items.length; i++) {
      const item = items[i];
      const name = item.name;
      const expand = item.expand;

      if (!name || !expand)
        FAIL("Shortcut must contain 'name' and 'expand' properties");

      const obj = {
        name  : name,
        expand: expand,
        doc   : item.doc || ""
      };

      this._shortcuts[name] = obj;
    }
  }

  _addInstructions(instructions) {
    FAIL("ISA._addInstructions() must be reimplemented");
  }

  _addInstruction(instruction) {
    let group;

    if (Object.hasOwn(this._instructionMap, instruction.name)) {
      group = this._instructionMap[instruction.name];
    }
    else {
      group = new InstructionGroup();
      this._instructionNames = null;
      this._instructionMap[instruction.name] = group;
      this.stats.groups++;
    }

    if (instruction.aliasOf) {
      this._addAlias(instruction.name, instruction.aliasOf);
    }

    group.push(instruction);
    this.stats.instructions++;
    this._instructions = null;

    return this;
  }

  // Add aliases from instruction database - aliases must be an object where each key is a non-aliased instruction.
  _addAliases(aliases) {
    for (let instructionName in aliases) {
      const data = aliases[instructionName];
      for (let aliasName of data.aliases) {
        this._addAlias(instructionName, aliasName, data.format || "");
      }
    }
  }

  _addAlias(instructionName, aliasName, aliasFormat) {
    const group = this._instructionMap[instructionName];
    if (!group) {
      FAIL(`Instruction ${instructionName} doesn't exist when processing alias (${aliasName})`);
    }

    let alias = this._aliases[instructionName];
    if (!alias) {
      alias = dict({
        primaryName: instructionName,
        aliasNames: [],
        format: ""
      });
      this._aliases[instructionName] = alias;
    }

    this._aliasMap[aliasName] = instructionName;
    alias.aliasNames.push(aliasName);
    alias.format = aliasFormat || "";
  }
}
base.ISA = ISA;

}).apply(this, typeof module === "object" && module && module.exports
  ? [module, "exports"] : [this.asmdb || (this.asmdb = {}), "base"]);
